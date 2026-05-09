# Event Triggers

Most "scripted" things that happen in a room (an enemy ambush spawning, a cutscene firing, Issun dialogue) are driven by trigger zones declared in the room's data file. The engine watches the player's position each tick, and when the player enters a zone, dispatches to a compiled C++ handler in `main.dll` keyed off the zone's parameters. There is no embedded script language; the zones are static records and the handlers are baked in.

This doc covers the static on-disk format. For the runtime side — how the per-stage TICK reads state bits, dispatches handlers via the scheduler, and what we figured out while bypassing Cave of Nagi's forced-tutorial softlock — see [event-triggers-runtime.md](event-triggers-runtime.md).

## Where the data lives

Per-room data sits under `data_pc/stN/rXXX.bin`, where the stage subdirectory `stN` groups rooms by chapter and `rXXX` is the room ID in hex (e.g. Cave of Nagi is `0x101`, so its file is `data_pc/st1/r101.bin`). Companion files alongside each `.bin` carry localized strings and the heavier per-room asset blob (`r101.dat`, ~MBs of model and texture data). The `.bin` is the lightweight scene description; trigger zones live there.

All of these files are Blowfish-encrypted (key `YaKiNiKuM2rrVrPJpGMkfe3EK4RbpbHw`, ECB, 8-byte blocks). After decryption the room file is a section-tagged container:

```
+0x00  uint32        section count N
+0x04  uint32[N+1]   section start offsets (last is end-of-data sentinel)
+0x40  char[4][N]    4-byte type tags ("ACT\0", "SCA\0", ...)
+0x80  ...           section data
```

Trigger zones live in the section tagged `SCA`. Other tags in the same file carry the room's actor instances (`ACT`), dialogue strings (`MSD`), item placements (`ITS`), and so on. This doc covers SCA only.

## SCA section header

```
+0x00  char[4]   magic "SCA\0"
+0x04  uint8     flag_a (often 0)
+0x05  uint8     flag_b (often 1)
+0x06  uint16    entry count
+0x08  uint8[16] reserved/zero
+0x18  ...       entries (176 bytes each)
```

The two flag bytes don't have known meanings; they appear constant across rooms.

## Entry layout

Each SCA entry is 176 bytes (`0xB0`). Field positions are confirmed; the field *semantics* are inferred from observed values and not all of them are pinned down.

```
+0x00  uint8     type
+0x01  uint8     subtype
+0x02  uint16    ?
+0x04  float     f1 (unknown; possibly a vertical bound or rotation)
+0x08  float     f2 (unknown)
+0x0C  uint32    zero/reserved
+0x10  float[8]  4 (x, z) ground-plane corners of the trigger quad
+0x30  uint8[8]  event params (handler dispatch info; see below)
+0x38  uint8[120] padding/reserved
```

The eight floats at `+0x10` decode cleanly as four `(x, z)` pairs that trace a quadrilateral on the ground plane. Y is the up-axis in Okami and isn't part of the quad; vertical extent is presumably supplied by `f1` / `f2` or unbounded. Real-world quads are usually convex but rarely axis-aligned, so don't reduce them to AABBs unless you only need a rough position.

## Event params

The 8-byte block at `+0x30` is the handler dispatch info. The pattern observed across an entire room's entries:

```
[ 0] 03 00 00 00 00 08 00 02
[ 1] 03 00 01 00 00 08 00 02
[ 2] 03 00 02 00 00 08 00 02
...
[ N] 03 NN NN 00 00 08 00 02
```

Best-guess decode:

| Bytes   | Meaning                                                      |
| ------- | ------------------------------------------------------------ |
| `+0x00` | Handler type (constant per room, often `0x03`)               |
| `+0x01` | Subtype (most entries 0; some entries differ, see below)     |
| `+0x02` | uint16 trigger ID; sequential and matches the entry index    |
| `+0x04` | uint8 (often 0)                                              |
| `+0x05` | uint8 group code (often `0x08`)                              |
| `+0x06` | uint16 terminal flag (commonly `0x02`; differs for outliers) |

The trigger ID matching the entry's array position is the most useful invariant: if a handler reads `volume[N]` and dispatches by ID, the index maps directly to whichever scene that zone fires.

## Reading a dump

A few heuristics make a SCA dump easier to interpret without ground-truthing every entry by walking around the room.

**Outlier params.** The subtype byte and the terminal-flag byte differ for a handful of entries per room. In one observed room the last entry had `subtype=1` and terminal flag `0x01` while every preceding entry had `subtype=0` and `0x02`. Outliers like this correlate with structurally different events (a "completion exit" rather than a "fire scene", for example). When skimming, those byte differences are the first place to look for entries doing something distinct from their neighbors.

**Geographic clustering.** Trigger entries within a room tend to cluster geographically because the level designers placed them in the same region of the map. Sort entries by their average `(x, z)` and look for clusters separated by long jumps. Each cluster usually corresponds to a logical sub-area: an outdoor approach versus an indoor chamber, an enemy ambush lane versus a cutscene chamber, the spawn lane the player walks through on the way back out versus the original-direction triggers.

**Overlapping pairs.** Two entries with overlapping AABBs are usually paired (a small "you crossed it" trigger inside a larger "you're still in the area" maintainer is a common shape). When suppressing one, check whether its sibling needs to go too.

## Confidence and gaps

Confirmed by inspection:

- Container encryption (Blowfish + section table) and section type tags.
- SCA entry stride (176 bytes) and the sequential trigger ID in the params block.

Inferred but not confirmed:

- Field semantics of the entry header (`f1`, `f2`, type/subtype byte ordering).
- The exact dispatch function in `main.dll` that consumes SCA entries; we know it must exist but haven't located it.
