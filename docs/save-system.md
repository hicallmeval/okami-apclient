# Save System

`SaveMan` is the subsystem that takes Okami's save pipeline and quietly replaces it. It exists because vanilla Okami stores progress in Steam Cloud-backed slots that the player shares across all their playthroughs; if AP wrote into the same slots a multiworld run would clobber the player's regular save, and vice versa. The mod sidesteps the problem by maintaining its own per-slot blob, a `.OKAMI` file in `%APPDATA%/okami-apsaves/`, and gating the vanilla save UI when AP is connected.

Files: [`src/okami-apclient/saveman.h`](../src/okami-apclient/saveman.h), [`saveman.cpp`](../src/okami-apclient/saveman.cpp).

## What's captured

A snapshot dumps six memory regions verbatim. They live at known offsets in `main.dll`:

| Region           | Type                     | Notes                           |
| ---------------- | ------------------------ | ------------------------------- |
| `CharacterStats` | `okami::CharacterStats`  | Health, food, praise, weapons   |
| `TrackerData`    | `okami::TrackerData`     | Time played, run-level tracking |
| `CollectionData` | `okami::CollectionData`  | Money, inventory item counts    |
| `MapState[]`     | `okami::MapState` array  | Per-map flags, world-state bits |
| `dialogBits`     | `okami::BitField<512>[]` | Dialog-seen flags per map       |
| `CustomTextures` | raw bytes                | Player-painted textures         |

These are exactly the regions the vanilla save pipeline writes; SaveMan reads/writes them directly through the addresses resolved in `initialize()`.

## Save-file layout

A `.OKAMI` file is a single `okami::SaveSlot` blob. The struct lives in [`include/okami/savefile.hpp`](../include/okami/savefile.hpp). The header includes a magic value, an areaNameID label, and a Windows FILETIME timestamp; the body is the six regions concatenated in the order above. A checksum at offset `+0x08` covers everything except the checksum field itself.

```
slot[0]:        SaveSlot header (magic, areaNameID, timestamp)
slot[+0x08]:    checksum (XOR over the rest)
slot[+0x10..]:  CharacterStats / TrackerData / ... / CustomTextures
```

### Checksum

`SaveMan::computeChecksum` is a 64-bit XOR with seed `0x9be6fa3b72afda1d`, walking the slot in 8-byte words and skipping the checksum field at `+0x08`. Same algorithm as the game's vanilla save format, so an `.OKAMI` is byte-compatible with the in-memory shape the game expects to find when the read hook serves it back.

## Hook architecture

SaveMan installs four hooks in `installHooks()`:

1. **`hookSaveGate`** at `cMcSave` vtable offset `+0x1c37d0`. The vanilla save state machine calls a gate function at the start of `cMcSave::Update`; returning 0 skips the entire save UI. When AP mode is active the gate returns 0, so the player never sees the vanilla save-slot picker. (Hooked because the alternative — letting the vanilla UI run — would race against the AP write and likely corrupt either or both.)

2. **`hookMcSaveCtor`** on the `cMcSave` constructor. Fires before the vanilla state machine has populated its working buffers. We snapshot AP state to `.OKAMI` here; the vanilla constructor still runs afterwards but has no save UI to drive (the gate takes care of that).

3. **`hookOkamiPureRead`** on the OKAMI binary-blob read function. The game asks for the contents of a save slot through this function; when AP is connected we serve `.OKAMI` bytes directly into the user buffer instead of letting the vanilla path round-trip through Steam Cloud. This is what lets us load AP saves: from the game's perspective, it asked Steam for a slot and got back exactly the bytes it expected.

4. **`installSteamRedirect`** patches `ISteamRemoteStorage` vtable methods (`FileWrite`, `FileWritten`, etc.) to no-ops while the redirect is active. With the redirect on, no AP gameplay state ever reaches Steam Cloud; with it off, the game runs unmodified. The redirect activates when SaveMan sees a connected socket and a non-empty save path, and deactivates on `onReturnToMenu` so that returning to the main menu re-exposes vanilla saves.

The four hooks together mean: when AP is connected, the game writes/reads what looks like a Steam Cloud save but is actually the `.OKAMI` file we control. When AP is not connected, every hook falls through to its original implementation; the player's vanilla saves are untouched.

## Auto-save policy

`CheckMan` calls `SaveMan::queueAutoSave()` after every check is sent. The actual write is deferred to the next game tick by `processAutoSave()`, which:

1. Checks `isSafeToSave()`. The save is deferred (not abandoned) if any of the following is true:
   - AP mode is not active.
   - `mapId` is the title screen or title-screen demo cutscene (no gameplay state to snapshot).
   - `systemFlags` bit 22 is set — `cMcSys`'s "busy" bit, raised whenever `cMcSave` / `cMcLoad` / `cMcBoot` is running. This is the only reliable busy signal we have; bits 6 and 8 of `saveStateFlags` look tempting but are sticky / transient signals that don't track in-flight saves.
   - `areaLoadFlags` has any bit in mask `0x6001000` set — a map transition is in progress. Saving mid-transition can race the engine.
2. Honours a debounce of 500ms so check bursts coalesce into one write.
3. If a save has been deferred for more than 30 seconds, logs a single warning so an operator notices the game is in a state that's been blocking saves longer than expected.

Saves can also be triggered explicitly through the login window or via console commands; those skip the queue and call `saveGameState()` directly. Manual saves still respect `isSafeToSave()` to avoid corruption.

## Save key

The `.OKAMI` filename is the slot's connection info: `{slot}_{seed}.OKAMI`. Two players on the same seed have different slots and therefore different files; the same player on two different seeds gets two files. The format matches `ArchipelagoSocket::getConnectionInfo()`.

## Failure modes

The most common surfaces, and how to recognize them in the log:

- **`hookMcSaveCtor fired with g_saveMan=null`** — the cMcSave constructor ran before `lateGameInit` finished wiring SaveMan. Should be impossible in practice; if it happens, the mod failed to fully initialize.
- **`OKAMI pure-read: redirect INACTIVE but AP mode ACTIVE`** — the read hook fired but the Steam redirect isn't on. Happens if the player connects after the title menu has already populated with vanilla state, then returns to the menu without a full reconnect; the redirect re-arms on `onReturnToMenu`.
- **`Checksum mismatch (file=0x..., computed=0x...) -- loading anyway`** — the saved checksum doesn't match what we recompute. Usually means the file was truncated or partially overwritten. We load it anyway; if the player's progress looks wrong, this is the line to check.
- **`Auto-save deferred >30s`** — gameplay has been in a state where `isSafeToSave()` returns false for long enough that we want a human to notice. Usually a long map transition or a stuck save flag.
- **`SaveSlot size mismatch -- expected 0x172A0 bytes`** — compile-time failure; the static-asserts in `saveman.cpp` would have already caught it. If you see this line at runtime someone has shipped a build where the struct definitions disagree with what the game expects.

## Testing

`tests/test_saveman.cpp` runs against the in-tree mock framework. The pattern is to `wolf::mock::reserveMemory(...)` for the save regions, write distinctive sentinel values through `apgame::*` accessors, call `saveGameState()` / `loadGameState()`, and assert byte-for-byte round-trip. The tests don't exercise the cMcSave gate hook directly (it requires the real game's state machine); the logical save/load pair has good coverage.
