# Server Protocol Reference

What the okami-apclient sends and expects on the network socket. If you're implementing or maintaining the Okami APWorld, this file is the contract you need to satisfy: location and item ID schemes, slot_data keys, version compatibility, and the recovery flows the client relies on.

The client speaks the standard Archipelago protocol via apclientpp, so this document only covers Okami-specific details. For general AP semantics see the [Archipelago Network Protocol](https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/network%20protocol.md) reference.

## Connection

- **Transport**: WebSocket. `ws://` for localhost, `wss://` for remote (the client upgrades automatically).
- **Default port**: 38281.
- **Message format**: JSON, handled by apclientpp.

The client requests `items_handling = 0b111 = 7` (receive items for self, receive items for others, starting inventory).

## Location ID scheme

Locations are int64s, as defined by the AP spec. Eight categories sit `1e9` apart, with `*1000` slots per inner key (mapId, shopId, levelId). The constants and helpers live in [`src/okami-apclient/checks/check_types.hpp`](../src/okami-apclient/checks/check_types.hpp); read it for the source of truth.

| Category           | Base            | Formula                          | Inner range       |
| ------------------ | --------------- | -------------------------------- | ----------------- |
| Brush acquisition  | `1_000_000_000` | `base + brushIndex`              | per brush         |
| Shop purchase      | `2_000_000_000` | `base + shopId*1000 + itemSlot`  | `slot < 1000`     |
| World state        | `3_000_000_000` | `base + mapId*1000 + bitIndex`   | `bit < 1000`      |
| Collected object   | `4_000_000_000` | `base + mapId*1000 + bitIndex`   | `bit < 1000`      |
| Area restored      | `5_000_000_000` | `base + mapId*1000 + bitIndex`   | `bit < 1000`      |
| Global flag        | `6_000_000_000` | `base + bitIndex`                | per bit           |
| Game progress flag | `7_000_000_000` | `base + bitIndex`                | per bit           |
| Container pickup   | `8_000_000_000` | `base + levelId*1000 + spawnIdx` | `spawnIdx < 1000` |

Worked examples:

- Brush 5 (Greensprout): `1_000_000_005`.
- Shop ID 3, slot 7: `2_000_003_007`.
- Map 12, world-state bit 42: `3_000_012_042`.
- Level 5, container spawn index 8: `8_000_005_008`.

The categories sit far enough apart that `getCheckCategory(id)` can decode by simple range comparison. The bit-index conversion between WOLF's monitor (LSB-0) and the game's `BitField<N>` (MSB-0 within each 32-bit word) is handled by `monitorToGameBitIndex` — APWorld implementers report the WOLF index, so they don't need to think about it directly.

---

## Item ID scheme

Items are int64s. Ranges are defined in [`src/okami-apclient/rewards/reward_types.hpp`](../src/okami-apclient/rewards/reward_types.hpp).

| Range           | Category            | Mapping                                             |
| --------------- | ------------------- | --------------------------------------------------- |
| `0x00`-`0xFF`   | Game items          | AP ID == game item ID, granted via `wolf::giveItem` |
| `0x100`-`0x11E` | Brushes             | `brushIndex = apItemId - 0x100`                     |
| `0x300`-`0x302` | Progressive weapons | Multi-tier; see below                               |
| `0x303`-`0x308` | Event flags         | Story progression bits                              |

IDs outside these ranges are treated as `RewardCategory::Unknown` and logged as an error.

### Progressive weapons

Each progressive weapon advances one stage per receipt. Stage indexing is in `rewards::getCategory` and the weapon handler.

| AP ID   | Item               | Stages                                                                                                              |
| ------- | ------------------ | ------------------------------------------------------------------------------------------------------------------- |
| `0x300` | Progressive Mirror | Trinity Mirror (`0x13`) -> Solar Flare (`0x14`)                                                                     |
| `0x301` | Progressive Rosary | Devout (`0x15`) -> Life (`0x16`) -> Exorcism (`0x17`) -> Resurrection (`0x18`) -> Tundra (`0x19`)                   |
| `0x302` | Progressive Sword  | Tsumugari (`0x1A`) -> Seven Strike (`0x1B`) -> Kusanagi (`0x1C`) -> Eighth Wonder (`0x1D`) -> Thunder Edge (`0x1E`) |

### Progressive brushes

Power Slash and Cherry Bomb have multi-stage upgrades within the brush range. The first receipt grants the base technique; subsequent receipts set upgrade bits in the brush bitfield (which is big-endian within each word — see `okami::BitField<N>`).

| AP ID   | Brush       | Stages                                              |
| ------- | ----------- | --------------------------------------------------- |
| `0x102` | Power Slash | base -> PS2 (upgrade bit 0) -> PS3 (upgrade bit 10) |
| `0x103` | Cherry Bomb | base -> CB2 (upgrade bit 6) -> CB3 (upgrade bit 11) |

### Item flags

The standard AP `flags` bitfield is honoured:

| Bit   | Name        | Effect                                         |
| ----- | ----------- | ---------------------------------------------- |
| `0x1` | Progression | Item icon gets the progression treatment       |
| `0x2` | Useful      | (Reserved; no special handling beyond display) |
| `0x4` | Trap        | Item icon gets the trap treatment              |

---

## slot_data

Sent in `Connected.slot_data`. The client parses these keys in `SlotConfig::parse`. Missing or malformed fields fall back to the listed default rather than failing the connection, so an old client can connect to a newer server's slot as long as the server-required keys are present.

### Session info

| Key                        | Type           | Default | Notes                                                         |
| -------------------------- | -------------- | ------- | ------------------------------------------------------------- |
| `SeedNumber`               | string         | `""`    | Unique seed identifier; baked into save filenames             |
| `SeedName`                 | string         | `""`    | Human-readable seed label                                     |
| `TotalLocations`           | int (optional) | unset   | If set, used to render check-count progress                   |
| `supported_client_version` | string         | `""`    | Minimum client semver the APWorld expects (see compatibility) |

### Randomization toggles

| Key                   | Type | Default |
| --------------------- | ---- | ------- |
| `RandomizeContainers` | bool | `false` |
| `RandomizeShops`      | bool | `false` |
| `RandomizeBrushes`    | bool | `false` |

### General options

| Key                     | Type | Default | Notes                                                |
| ----------------------- | ---- | ------- | ---------------------------------------------------- |
| `BuriedChestsByNight`   | bool | `true`  | Buried chests logically require Crescent             |
| `KarmicTransformers`    | int  | `1`     | `0`=excluded, `1`=precollected, `2`=in item pool     |
| `OpenGameStart`         | bool | `false` | Skip early-game cutscenes for an open start          |
| `ProgressiveWeapons`    | bool | `false` | Use progressive weapon items vs. individual upgrades |
| `RemoveBlockHead`       | bool | `true`  | Remove Blockhead encounters                          |
| `BloomGuardianSaplings` | bool | `true`  | Bloom guardian saplings at start                     |

### Orochi arc

| Key                | Type | Default | Notes                                              |
| ------------------ | ---- | ------- | -------------------------------------------------- |
| `RequiredDoggorbs` | int  | `1`     | Canine warriors needed for Gale Shrine (range 1-8) |
| `CanineRewards`    | int  | `1`     | `0`=vanilla, `1`=randomized, `2`=junk              |
| `MoonCaveAccess`   | int  | `0`     | `0`=serpent_crystal, `1`=crimson_helm, `2`=open    |

### Other

| Key         | Type | Default |
| ----------- | ---- | ------- |
| `ShopSlots` | int  | `6`     |

---

## Version compatibility

The client reports its semver in `Connect.version`. The server's expected version is in `slot_data.supported_client_version`. Compatibility is decided by `version_utils::checkCompatibility`:

- **Major version mismatch**: incompatible. Connection refused with a clear message.
- **Major 0.x.y (pre-release)**: minor versions must match exactly. Pre-1.0 the API is unstable, so every minor bump is a potential break.
- **Major 1+**: client's minor must be `>=` server's. A client running 1.3.x can connect to a slot that needed 1.2.x; the reverse fails.
- **Patch and pre-release suffixes** (`-dev.21`, `+build.456`) are parsed but ignored for compatibility decisions.

When publishing an APWorld, set `supported_client_version` to the lowest client semver that has the features you depend on.

---

## Recovery flows

### Connection timeout

The client waits 10 seconds for `Connected` after sending `Connect`. If it doesn't arrive, the login window reports the connection as failed; nothing is persisted.

### Item index gaps

Gaps between `ReceivedItems.items[*].index` values mean the client missed a packet. The client sends `Sync` and the server replays the full inventory with `index: 0`. Already-processed items are skipped via the on-disk index.

### Reconnect

1. Client loads the last processed item index from `%APPDATA%/okami-apsaves/{slot}_{seed}.save`.
2. Server sends full inventory (`index: 0`).
3. Client skips items at or below the saved index.
4. Client processes new items.
5. Client merges its `checked_locations` cache with the server's and resends any local-only entries.

Same-saved-index reconnects are idempotent; the client tolerates seeing every item twice.

---

## See also

- [Architecture](architecture.md) — code-side model.
- [Mod flow](mod-flow.md) — runtime trace from launch to first check.
