#include "console_commands.h"

#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <wolf_framework.hpp>

#include "isocket.h"
#include "rewardman.h"
#include "slotconfig.h"

namespace console_commands
{

namespace
{

// Parse a signed 64-bit integer from a string. Accepts decimal and the 0x / 0X
// hex prefix. Returns true on success.
bool parseInt64(std::string_view s, int64_t &out)
{
    if (s.empty())
        return false;

    int base = 10;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
    {
        s.remove_prefix(2);
        base = 16;
    }

    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out, base);
    return ec == std::errc{} && ptr == s.data() + s.size();
}

void cmdStatus(ISocket &socket, const std::vector<std::string> & /*args*/)
{
    wolf::consolePrintf("[AP] connected=%d  status=%s", socket.isConnected() ? 1 : 0, socket.getStatus().c_str());
    if (socket.isConnected())
    {
        wolf::consolePrintf("[AP] %s", socket.getConnectionInfo().c_str());
        wolf::consolePrintf("[AP] slot=%d  uuid=%s", socket.getPlayerSlot(), socket.getUUID().c_str());
    }
    if (socket.isSlotConfigReady())
    {
        const auto &cfg = socket.getSlotConfig();
        wolf::consolePrintf("[AP] slot_data: shops=%d brushes=%d containers=%d shopSlots=%d", cfg.randomizeShops ? 1 : 0, cfg.randomizeBrushes ? 1 : 0,
                            cfg.randomizeContainers ? 1 : 0, cfg.shopSlots);
    }
}

void cmdSay(ISocket &socket, const std::vector<std::string> &args)
{
    // args[0] is the command name; everything after is the message body.
    if (args.size() < 2)
    {
        wolf::consolePrint("[AP] usage: ap say <text>  (e.g. \"ap say /help\")");
        return;
    }
    if (!socket.isConnected())
    {
        wolf::consolePrint("[AP] not connected; cannot send");
        return;
    }

    std::string text;
    for (size_t i = 1; i < args.size(); ++i)
    {
        if (i > 1)
            text.push_back(' ');
        text += args[i];
    }
    if (!socket.say(text))
        wolf::consolePrint("[AP] say failed (check connection state)");
}

void cmdGive(RewardMan &rewardMan, ISocket &socket, const std::vector<std::string> &args)
{
    if (args.size() < 2)
    {
        wolf::consolePrint("[AP] usage: ap give <ap_item_id>  (decimal or 0xHEX)");
        return;
    }

    int64_t itemId = 0;
    if (!parseInt64(args[1], itemId))
    {
        wolf::consolePrintf("[AP] could not parse item id %s", args[1].c_str());
        return;
    }

    // Resolve a friendly name through the socket if connected; fall back to a
    // marker so logs are searchable.
    std::string name = socket.isConnected() ? socket.getItemName(itemId, socket.getPlayerSlot()) : std::string{};
    if (name.empty())
        name = "console-grant";

    // Route through the same queue the items_received handler uses, so the
    // grant exercises RewardMan + the brush/game-item/event-flag dispatchers
    // exactly as a real server delivery would.
    rewardMan.queueReward(itemId, name, 0);
    wolf::consolePrintf("[AP] queued reward 0x%llX (%s)", static_cast<unsigned long long>(itemId), name.c_str());
}

void cmdHelp()
{
    wolf::consolePrint("[AP] subcommands:");
    wolf::consolePrint("[AP]   ap status               - print connection / slot info");
    wolf::consolePrint("[AP]   ap say <text>           - send chat or server slash command (e.g. /help)");
    wolf::consolePrint("[AP]   ap give <ap_item_id>    - locally queue a reward (decimal or 0xHEX)");
}

} // namespace

void registerAll(ISocket &socket, RewardMan &rewardMan)
{
    wolf::addCommand(
        "ap",
        [&socket, &rewardMan](const std::vector<std::string> &args)
        {
            if (args.size() < 2)
            {
                cmdHelp();
                return;
            }
            const std::string &sub = args[1];
            // Reslice so handlers see args[0] = subcommand, matching the usual
            // CommandHandler convention.
            std::vector<std::string> subArgs(args.begin() + 1, args.end());

            if (sub == "status")
                cmdStatus(socket, subArgs);
            else if (sub == "say")
                cmdSay(socket, subArgs);
            else if (sub == "give")
                cmdGive(rewardMan, socket, subArgs);
            else if (sub == "help")
                cmdHelp();
            else
            {
                wolf::consolePrintf("[AP] unknown subcommand '%s'", sub.c_str());
                cmdHelp();
            }
        },
        "Archipelago client commands. Try 'ap help'.");
}

} // namespace console_commands
