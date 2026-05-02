# Using the Mod

The first time you launch Okami HD with the loader running, you'll see the game start normally. If the AP connection window does not appear, press **F2** any time during the title screen or in-game to bring it up. Everything else lives behind that one window and a couple of keybinds.

## Connecting to a slot

Bring up the login window with **F2** and fill in:

- **Server**: the room URL or `host:port`. The mod adds `ws://` for localhost and `wss://` for everything else automatically.
- **Slot**: your slot name from the YAML / room configuration.
- **Password**: the room password if there is one; leave blank otherwise.

Click **Connect**. If everything's right you'll see the status flip to `Connected`. Your previous connection is remembered, so the next time you launch you can just hit Connect.

If a connection fails, the status line says why. Common causes:

- "Connection refused: `InvalidSlot`" — slot name doesn't match the room.
- "Connection refused: `IncompatibleVersion`" — the server expects a newer client than yours; check the [releases page](https://github.com/Axertin/okami-apclient/releases).
- Connection times out — server URL is wrong, the server is down, or your network is down.

## Saves and the save picker

The mod keeps Archipelago saves separate from your vanilla saves. While AP is connected:

- The mod writes a single `.OKAMI` file per slot/seed at `%APPDATA%/okami-apsaves/{slot}_{seed}.OKAMI`.
- Auto-save runs after every check is sent. If gameplay isn't in a stable state (loading screen, save-in-flight, title menu), the auto-save defers until it is.

When you exit to the title menu, vanilla saves become visible again. The mod only intercepts saves while you're playing an AP session.

## Keybindings

| Key                         | What it does                                                        |
| --------------------------- | ------------------------------------------------------------------- |
| **F2**                      | Toggle the Archipelago login window                                 |
| **HOME**                    | Toggle the entire ImGui UI (notifications, login, warp, everything) |
| **END** / **ALT** / **WIN** | Unlock the cursor from the game so you can interact with the UI     |

If the UI is hidden via HOME, the F2 toggle won't make the login window appear; press HOME first.

## Console commands

The WOLF dev console exposes a few commands for connection diagnostics, chat, and debug item grants. Open the console (look for the WOLF docs for the keybind on your install) and try:

```
ap help
```

That lists the subcommands. The most useful are:

- **`ap status`** — prints connection state, slot, UUID, and the active randomization options. First thing to run when something looks wrong.
- **`ap say <text>`** — sends a chat message to the AP room. Use this to send server slash-commands too: `ap say /help`, `ap say /hint Power Slash`, etc.
- **`ap give <ap_item_id>`** — locally queues a reward as if the server had sent it. Item ID can be decimal (`256`) or hex (`0x100`). Useful for debugging or testing reward handlers.

`ap give` routes the grant through the same dispatcher real server deliveries use, so it's a faithful test of brush / item / event-flag handlers.

## Troubleshooting

**Items aren't being granted.** Check `ap status` — if `connected=0` the socket has dropped; reconnect via the login window.

**The chest opened but I didn't get a check.** Check the in-game console for `[CheckMan]` log lines. If the check was sent (`Sent check: ...`), the server received it; the issue is on the APWorld side. If you don't see the line, container randomization may not be enabled in your slot — verify with `ap status`.

**My save didn't load.** AP saves are keyed by `{slot}_{seed}`. If you connect to a different slot or a re-rolled seed, the mod writes a new file rather than reading the old one. Check `%APPDATA%/okami-apsaves/` for files matching your current slot and seed.

**Game crashes when I attach a debugger.** This is Windows TDR resetting the GPU. The fix is documented in [development.md](development.md#tdr-why-debugger-attach-crashes-the-game).

**Item names in the shop are placeholders / look wrong.** That can happen if the server never told the client what items are in it. If they stay placeholder, the room may not have responded to the scout; reconnecting usually clears it.

**Where are the logs?** The mod writes timestamped logs to `logs/` next to the game executable. Always include the relevant log when filing a bug.

## Reporting bugs

If something's wrong, file an issue using the [bug report template](https://github.com/Axertin/okami-apclient/issues/new?template=bug_report.md). The most useful things to include:

- Your client version (run `ap status` or check the login window title).
- The Archipelago server you connected to and your slot/seed if it's not private.
- The relevant section of `logs/` covering the failure.
- A short description of what you expected vs. what happened.

For general questions or to chat with other players, the [Archipelago Discord](https://discord.com/channels/731205301247803413/1196620860405067848) is the best place.
