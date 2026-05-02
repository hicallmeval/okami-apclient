# Runtime Trace

A walk through what happens at runtime, in order, from the moment the game launches with the mod loaded to the moment a check turns into a reward. Every step names the function and file you can step into. For component overviews see [architecture.md](architecture.md); for socket-format details see [server-protocol.md](server-protocol.md).

## 1. WOLF loads the DLL

The wolf-loader injects `okami-apclient.dll` into the Okami process, then calls into `WOLF_MOD_ENTRY_CLASS(APClientMod)` (registered at the bottom of [`okami-apclient.cpp`](../src/okami-apclient/okami-apclient.cpp)). WOLF gives us two init phases:

- **`earlyGameInit`** runs before main.dll's static constructors. Currently calls `itempatch::initializeEarly()`, which sets up the icon packages before the game reserves the memory for them (before `flower_init()`), ensuring that enough heap is reserved to store the larger custom packages.
- **`lateGameInit`** runs after main.dll is fully loaded. This is where everything else happens.

## 2. `lateGameInit` wires the managers

Construction order matters. Each step depends on what came before:

1. `apgame::initialize()` — bind the `MemoryAccessor<T>` singletons (`apgame::trackerData`, `apgame::brushUpgrades`, etc.). Everything else assumes these are valid.
2. `itempatch::initialize()` — install MSD-substitution and item-icon hooks.
3. `itempatch::patchItemParams()` — patch the ItemParam table. Runs *after* the hooks above so the patches catch the first frame.
4. `g_saveMan` constructed; `initialize()` resolves save-region addresses; `installHooks()` installs the cMcSave gate, the Steam redirect, and the pure-read hook.
5. `g_checkMan` constructed against `ArchipelagoSocket::instance()`.
6. `g_rewardMan` constructed with a callback that toggles `CheckMan::enableSending`. Granting a reward will later disable check sending for the duration of the grant; this is how we wire that.
7. `setRewardMan` / `setCheckMan` on the socket — incoming AP messages now have somewhere to dispatch.
8. UI initializes: `initializeGui`, `loginwindow::initialize`, `warpwindow::initialize`. The login window reads any persisted connection info from disk so the player can reconnect to their previous slot with one click.
9. `g_checkMan->initialize()` — registers the gameplay-state callbacks and constructs the per-category sub-handlers (`BrushMan`, `ContainerMan`, `ShopMan`).
10. `console_commands::registerAll` — `ap status` / `ap say` / `ap give` are now usable from WOLF's dev console.
11. The **game-tick handler** is registered last. It's the per-frame driver and runs on the main thread.

## 3. The login window

The player presses F2 to bring up the login window. They enter server address, slot, password, then click Connect. Internally `ArchipelagoSocket::connect` validates the URI, builds the apclientpp instance, and kicks off the WebSocket handshake on the apclientpp thread.

## 4. The handshake

When `Connected` arrives, two things land at once: the server's `checked_locations` list (so we can sync) and the `slot_data` blob. `SlotConfig::parse` decodes the latter; missing fields fall back to defaults so old clients can connect to newer slots.

## 5. The save handoff

If a `.OKAMI` file exists for `{slot}_{seed}`, SaveMan's pure-read hook serves its bytes the next time the game asks Steam Cloud for a slot and the player can only select their AP slot from the Continue menu. If no file exists, SaveMan creates one on the first auto-save.

When the player loads a save file, `wolf::onPlayStart` fires. SaveMan flips `apModeActive_` to true; CheckMan switches `enableSending(true)`.

## 6. The gameplay tick loop

Every frame, `wolf::onGameTick` calls (in order):

1. `socket.processMainThreadTasks()` — drain lambdas the apclientpp thread queued (item callbacks, scout responses).
2. `socket.poll()` — non-blocking apclientpp pump.
3. `g_rewardMan->processQueuedRewards()` — grant any rewards waiting in the queue.
4. `g_checkMan->poll()` — re-evaluate brush active state, tick the brush handler, poll containers.
5. (Steam-redirect activation if just connected; warns if the player connected post-title-menu.)
6. `g_saveMan->processAutoSave()` — write the auto-save if one is queued and `isSafeToSave()`.

## 7. The player triggers a check

The player does something the mod considers a check. Path depends on the source:

**Brush acquired.** The game flips a bit in the brush bitfield. `BrushMan` (running through a WOLF bitfield monitor) sees the 0->1 transition and calls `CheckMan::sendCheck(getBrushCheckId(brushIndex))`.

**Container opened.** At level load `ContainerMan` already replaced the container's contents with a dummy item via the spawn-table populator hook. When the player picks up the dummy, `wolf::onItemPickupBlocking` fires; `ContainerMan::shouldBlockItemPickup` returns true (so the game doesn't grant the dummy), and the corresponding container check ID is sent.

**Shop purchase.** `ShopMan`'s purchase hook fires inside `CKibaShop_GetShopStockList` / the purchase path. The slot-keyed shop check is sent; the actual purchased item is whatever the server scouted for that slot.

**World-state / collected-object / area-restored / global-flag / game-progress bit flips.** Same mechanism as brushes: a WOLF bitfield monitor sees the transition and the appropriate gamestate-monitor handler computes the check ID via `checks::getXCheckId(...)` and calls `sendCheck`.

`CheckMan::sendCheck` is the funnel. It checks `sendingEnabled_` and `socket_.isConnected()`, dedupes against `sentChecks_`, sends the location, marks it sent, and finally calls the auto-save callback so SaveMan queues a write.

## 8. The server replies

The AP server processes the location check, looks up what item lives at that location, and sends `ReceivedItems` back to the player who owns it (which may or may not be us). For us, the item arrives on the apclientpp thread; the callback queues `rewardMan.queueReward(apItemId)` for the main thread.

## 9. The reward grant

On the next game tick, `processQueuedRewards`:

1. Calls into the suppression callback, flipping `CheckMan::enableSending(false)`. Granting an item flips game state bits that would otherwise look like fresh checks.
2. Dispatches by ID range (`rewards::getCategory`). Game items go through `wolf::giveItem`; brushes flip the appropriate bits in the brush bitfield (via `Set()`, never raw); progressive weapons walk the upgrade chain by checking current state and granting the next stage; event flags flip a single bit.
3. Re-enables check sending.

Auto-save is queued.

## 10. Return to menu

`wolf::onReturnToMenu` fires when the player exits to the title screen. SaveMan deactivates the Steam redirect (vanilla saves become visible again) and flips `apModeActive_` to false; CheckMan switches `enableSending(false)`. The socket stays connected.

## 11. Reconnect

If the connection drops mid-session, the login window shows the disconnected status. On reconnect:

1. The socket reopens and receives a fresh `Connected`. Slot data is re-parsed.
2. `ArchipelagoSocket` reads the persisted item index from `%APPDATA%/okami-apsaves/{slot}_{seed}.save` and skips items it's already processed when the server replays the inventory at `index: 0`.
3. `CheckMan::syncWithServer` merges the server's `checked_locations` into the local cache and resends any local-only entries (covers the player completing a check while disconnected).
4. SaveMan's pure-read hook continues serving `.OKAMI` bytes; nothing on disk changes during a reconnect.

## 12. Recovery: index gaps

If `ReceivedItems.items` has gaps in the per-item index (we expected 42, got 45), the socket sends `Sync` and the server replays the full inventory at `index: 0`. The persisted index lets us skip what we've already processed; the rest gets queued through `RewardMan` normally.

---

## Worked example: opening a randomized chest

1. The player walks into a level. During the loading screen, the spawn-table populator hook fires; `ContainerMan` looks at each entry, decides it's a container and a known check, replaces the spawned item with a dummy.
2. The player opens the chest and the color-coded dummy model pops out and is collected.
3. The vanilla pickup path runs. Just before the game grants the dummy item, `wolf::onItemPickupBlocking` fires; `ContainerMan::shouldBlockItemPickup` returns true.
4. The corresponding container check ID is computed (`getContainerCheckId(levelId, spawnIdx)`) and sent. The auto-save callback queues a write.
5. The server resolves the location to whatever real item lives there, sends `ReceivedItems` to the appropriate player. If that's us, the item is queued for grant.
6. Next tick, `processQueuedRewards` flips check sending off, the reward handler grants the real item (e.g. Inkfinity Stone via `wolf::giveItem`), check sending flips back on, the auto-save runs.

The whole sequence runs on the main thread and completes within one or two ticks. There may be a delay between steps 4 and 5 due to network communication, but it does not block gameplay or any local threads (just acquisition of the item in inventory).
