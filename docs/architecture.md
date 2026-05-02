# Architecture

## Source layout

```
okami-apclient/
|-- src/okami-apclient/        # Archipelago-side: everything that talks to AP
|   |-- checks/                # Check-side category handlers
|   |-- rewards/               # Reward-side category handlers
|   |-- data/                  # Game-format binary helpers
|   |-- ui/                    # ImGui windows
|   |-- icons/                 # Source DDS files for the custom icon package
|
|-- include/okami/             # Game-side: types, layouts, constants
|
|-- tests/
|
|-- external/                  # Submodules: apclientpp, websocketpp, wswrap, imgui, vcpkg
|-- cmake/                     # SimpleGitVersion, llvm-mingw toolchain, helpers
|-- scripts/                   # Build-time code generation
|-- docs/                      # You are here
```

The `src/` vs `include/okami/` split is a hard rule: anything Okami-side (memory layouts, enums, MSD/save formats, item/brush/map types) lives in `include/okami/`; anything Archipelago-side (sockets, managers, UI) lives in `src/okami-apclient/`. New code that crosses the boundary almost always belongs on the AP side.

## Terminology

The mod renames two AP concepts to avoid confusion with native game vocabulary.

| AP term   | Mod term | Owner                                       |
| --------- | -------- | ------------------------------------------- |
| Locations | Checks   | `CheckMan` (`src/okami-apclient/checks/`)   |
| Items     | Rewards  | `RewardMan` (`src/okami-apclient/rewards/`) |

A "Check" is something the player did that we report to the server (game-state -> server). A "Reward" is something the server sent us that we apply in-game (server -> game-state). The directory split reflects this direction.

## Manager hierarchy

`lateGameInit` in `okami-apclient.cpp` constructs the managers in this order:

1. **`apgame::initialize()`** binds the `MemoryAccessor<T>` singletons used everywhere else. Must run first; everything else assumes accessors are valid.
2. **`itempatch::initialize()`** installs the MSD-substitution and item-icon hooks before `patchItemParams()` runs, so the patches catch the first frame of game state.
3. **`SaveMan`** (`saveman.h`) is created and its hooks installed (cMcSave gate, Steam-redirect interceptor). See [save-system.md](save-system.md) for the deep-dive.
4. **`CheckMan`** (`checkman.h`) is constructed against the `ArchipelagoSocket` singleton. Its sub-handlers — `BrushMan`, `ContainerMan`, `ShopMan`, plus the gamestate-bitfield monitors — are registered in `CheckMan::initialize()`.
5. **`RewardMan`** (`rewardman.h`) is constructed with a callback that toggles `CheckMan::enableSending`. The callback fires while a reward is being granted, so item grants don't echo back as new checks.
6. **Managers are injected into the socket** (`setRewardMan`, `setCheckMan`) so incoming AP messages can reach them.
7. UI windows (`loginwindow`, `warpwindow`, `notificationwindow`) initialize.
8. **`console_commands::registerAll`** registers `ap status` / `ap say` / `ap give` after managers exist, so handlers can close over live references.
9. The game-tick handler is the per-frame driver: `socket.processMainThreadTasks() -> socket.poll() -> RewardMan::processQueuedRewards() -> CheckMan::poll() -> SaveMan::processAutoSave()`.

Every manager exposes a `shutdown()` that mirrors `initialize()`. Destructors call it; `APClientMod::shutdown()` calls it explicitly so resources release before WOLF unwinds.

## Check categories and ID encoding

Eight categories sit `1e9` apart, with `*1000` slots per inner key. The bases and helpers are in [`src/okami-apclient/checks/check_types.hpp`](../src/okami-apclient/checks/check_types.hpp); always go through the helpers (`getBrushCheckId`, `getShopCheckId`, etc.) rather than re-encoding the offsets by hand. The full table lives in [server-protocol.md](server-protocol.md#location-id-scheme).

`monitorToGameBitIndex` converts between WOLF's bitfield-monitor index (LSB-0) and the game's `BitField<N>` index (MSB-0 within each 32-bit word). The two conventions disagree, so sites that bridge them must convert explicitly. This same big-endian-within-word rule is the single most-frequent footgun in the codebase and may drive a change in how `MemoryAccessor` interacts with BitFields in WOLF in the future. See `okami::BitField<N>`.

The category handlers are in `src/okami-apclient/checks/`:

- `brushes.*` — `BrushMan`. Hooks the brush-acquisition path.
- `containers.*` — `ContainerMan`. Hooks the spawn-table populator and the item-pickup-blocking callback; replaces randomized container contents with dummy items at spawn and intercepts pickup to send the corresponding check.
- `shops.*` — `ShopMan`. Hooks `GetShopVariation`, `LoadRsc`, and `CKibaShop_GetShopStockList`; rewrites shop inventory using server-scouted data and reports purchases as checks.
- `gamestate_monitors.*` — bitfield watchers for game progress, global flags, world-state, collected objects, area-restored. One handle per `BitField<N>` region; the callback sends the appropriate category-encoded check ID.

## Reward categories and grant flow

Rewards arrive on the apclientpp thread and are queued. `RewardMan::processQueuedRewards` runs on the game tick and dispatches by ID range (`rewards::getCategory` in [`reward_types.hpp`](../src/okami-apclient/rewards/reward_types.hpp)).

| Range           | Handler                   | Notes                                       |
| --------------- | ------------------------- | ------------------------------------------- |
| `0x00`-`0xFF`   | `rewards/game_items.cpp`  | Direct inventory items via `wolf::giveItem` |
| `0x100`-`0x11E` | `rewards/brushes.cpp`     | Brush techniques; some are progressive      |
| `0x300`-`0x302` | `rewards/game_items.cpp`  | Progressive weapons (Mirror, Rosary, Sword) |
| `0x303`-`0x308` | `rewards/event_flags.cpp` | Story progression flags                     |

Granting a reward calls back into `CheckMan::enableSending(false)` for the duration of the grant; the side-effects (item appearing in inventory, flags flipping) would otherwise look like fresh checks.

## WOLF integration surface

(Most) Game-facing interaction goes through WOLF. The complete API is one header: `include/wolf_framework.hpp`.

- **Memory access**: `MemoryAccessor<T>(moduleName, address)` — typed read/write at a known offset. Bound once in `apgame::initialize()`; prefer `apgame::trackerData->...` over raw pointer arithmetic everywhere else.
- **Hooks**: `wolf::hookFunction(module, offset, replacement, &original)` — MinHook under the hood. The mock framework intercepts these in tests via a hook registry.
- **Bitfield monitors**: `wolf::createBitfieldMonitor(addr, callback)` watches a contiguous bitfield for 0->1 transitions. Used for gamestate monitors.
- **Lifecycle callbacks**: `onGameTick`, `onPlayStart`, `onReturnToMenu`, `onItemPickupBlocking`, `onBrushEdit`. Registered once during `lateGameInit`. They execute on the game's main thread; **do not block them**.
- **ImGui**: `WOLF_IMGUI_BEGIN`/`END` macros plus `wolf::registerGuiWindow(name, render, visible)`.
- **Logging**: `wolf::logInfo` / `Debug` / `Warning` / `Error`. Console output is also surfaced via `wolf::consolePrintf` for the dev console.

## Save system

`SaveMan` (`src/okami-apclient/saveman.h`) intercepts the game's Steam Cloud save/load pipeline and replaces it with `.OKAMI` files in `%APPDATA%/okami-apsaves/`. It also bypasses the vanilla save UI when AP is connected (cMcSave gate hook). Auto-save runs after every check is sent, but defers if a save is already in flight or the player is on the title/load screens. Full mechanics, blob format, and checksum algorithm in [save-system.md](save-system.md).

## Item-name substitution (MSD)

`itempatch.cpp` replaces strings in the game's MSD localization tables so AP dummy items display the actual scouted item name instead of a placeholder. Two strId paths matter:

- `294 + itemType` — the **shop row LIST** strId. Shared across every visible row of the same dummy type, so it must NOT be slot-keyed; substituting per-slot here causes every row to show whichever slot we resolved last.
- `0x2000 + itemType` — the **bottom INFO PANEL** strId. Shows the currently-selected slot only, so it's safe to resolve per-slot.

Substitution is **gated by context**, not just by strId range. The hook reads the live "Item shop menu" GlobalGameState bit (idx 10) and the strId-path predicate before replacing. Not doing this and returning custom names for any strId `>= kCustomStringBase` collides with cutscene banners and brush textboxes, because the game allocates strIds in that same range.

## Custom resource packages

The game ships a Blowfish-encrypted `.dat` package per resource family (icons, models). To display arbitrary AP item icons and models, the mod builds custom packages that overlay the vanilla ones:

- `data/customiconpkg.cpp` — bakes a custom icon `.dat` containing AP-themed variants (standard / progression / trap) and ships it via `archipelago/customicons.dat`. Source DDS files in `src/okami-apclient/icons/`.
- `data/custommodelpkg.cpp` — analogous for 3D models used by AP dummy entities.
- `data/blowfish.cpp` — encryption helpers, used transparently by both.
- `data/resourcepkg.cpp` — shared `.dat` package reader.

Build failures here fall back to a vanilla item type (chestnut) rather than crashing the game.

## Slot config

`SlotConfig` (`src/okami-apclient/slotconfig.h`) is the parsed `slot_data` blob from the `Connected` packet: randomization toggles, shop slot count, seed identifiers, and other options. Missing or malformed fields fall back to "safe" defaults rather than failing the connection. Full key list and defaults in [server-protocol.md](server-protocol.md#slot_data).

## Threading

Two threads matter:

- **Main (game) thread**: game tick, ImGui rendering, reward grants, check polling, save processing.
- **apclientpp thread**: WebSocket I/O, item/scout callbacks.

Cross-thread communication happens through `ArchipelagoSocket::processMainThreadTasks` — apclientpp callbacks queue lambdas, the game tick drains the queue. Direct synchronization is limited to a few mutexes (client, task queue, reward queue, scout result) plus atomic flags for connection state.

The hot rule: anything that runs on the main thread (`onGameTick`, `render`, hook callbacks installed in main-thread paths) must not block. Spawn a worker if you need network or disk I/O. There are exceptions to this, especially where pre-scouting locations has to happen during a loading screen or scene transition. In those cases, the socket is allowed to block the game. If blocked for too long though, Windows will halt the process, so thi should be avoided where possible. 

## Tests

Two Catch2 executables, both built by the `native-tests-debug` and `x64-clang-debug` presets:

- **`apclient-tests`** — unit suite. One `test_*.cpp` per component at the top of `tests/`. Uses the in-tree mock framework (`tests/mocks/wolf_framework.{hpp,cpp}`) which provides a fake `mockMemory` byte array, a hook registry, and stubs for `wolf::giveItem` and the lifecycle callbacks.
- **`apclient-harness-tests`** — end-to-end fixtures. Lives in `tests/harness/`. Closer to integration tests against the mock socket.

## Hazards

Things that have bitten me in the past:

1. **`okami::BitField<N>` is big-endian within each 32-bit word**: `mask = 0x80000000 >> (index % 32)`. Index 0 is the *high* bit. Always go through `Set(idx)` / `IsSet(idx)` / `Clear(idx)`; never bit-twiddle the underlying `values[]`. Anything that mirrors this layout (`apgame::brushUpgrades`, `WorldStateData::mapStateBits`) inherits the trap.
2. **Hot paths must not stall**: `onGameTick`, render callbacks, hook bodies installed on main-thread paths run on the game's main thread. Network and file I/O go on a worker; hand work back via `processMainThreadTasks`.
3. **MSD substitution is context-gated, not strId-range-gated**: see the [item-name substitution](#item-name-substitution-msd) section above and issue #113.
4. **TDR kills the game on debugger attach**: registry workaround documented in [development.md](development.md). Don't try to fix this in code.
5. **Cross-platform considerations**: avoid Windows-API calls Proton doesn't implement; the `llvm-mingw-cross-debug` preset will catch the link errors. Tests build on Linux without `__fastcall` (it's `#define`d to nothing in test builds), so keep typedefs for hooked function pointers compatible.

## See also

- [development.md](development.md) — toolchain, presets, writing tests.
- [mod-flow.md](mod-flow.md) — runtime trace from launch to first check.
- [server-protocol.md](server-protocol.md) — wire-format reference.
- [save-system.md](save-system.md) — SaveMan deep-dive.
