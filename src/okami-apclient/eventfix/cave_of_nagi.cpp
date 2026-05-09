#include "cave_of_nagi.hpp"

#include <okami/brushes.hpp>
#include <wolf_framework.hpp>

#include "common.hpp"

namespace eventfix::cave_of_nagi
{

namespace
{

// FUN_1804c1b10: entrance cutscene + portcullis-fall + script 0x1003.
// Pure suppression -- the camera-lower and portcullis animation are
// purely cosmetic; nothing else in CoN's chain depends on this firing.
void __fastcall stubEntrance()
{
    eventfix::clearCutsceneModeBits();
    wolf::logInfo("[eventfix] CoN entrance cutscene suppressed");
}

// FUN_1804c31c0: statue rejuv -> constellation puzzle -> kami dialog
// chain. Granting the brush bit fires BrushMan's watcher (which sends
// the AP check), and skipping the rest prevents +0x3A0 bit 10 (the
// FUN_1804c1f50 dispatch gate) from being set, so the kami dialog +
// forced rock-slash tutorial chain never fires.
//
// The natural FUN_1804c1f50 path also calls the same brush-bit-set; we
// bypass that path entirely, so this is the only grant that fires.
void __fastcall stubStatueRejuv()
{
    eventfix::clearCutsceneModeBits();
    eventfix::grantBrush(okami::BrushOverlay::power_slash);
    wolf::logInfo("[eventfix] CoN statue/kami suppressed; granted Power Slash");
}

constexpr EventBypass kBypasses[] = {
    {"CoN entrance cutscene", 0x4C1B10, stubEntrance},
    {"CoN statue rejuv chain", 0x4C31C0, stubStatueRejuv},
};

} // namespace

std::span<const EventBypass> getBypasses()
{
    return kBypasses;
}

} // namespace eventfix::cave_of_nagi
