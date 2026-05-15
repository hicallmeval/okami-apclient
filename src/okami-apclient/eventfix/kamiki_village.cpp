#include "kamiki_village.hpp"

#include <okami/brushes.hpp>
#include <wolf_framework.hpp>

#include "common.hpp"

namespace eventfix::kamiki_village
{

namespace
{

// FUN_1804c64a0: first link in the Sakuya descent -> Water Lily grant
// -> forced lily-pad tutorial chain. Calls FUN_18048c720(descriptor,
// 10/11/12) to step the stage descriptor into the tutorial transient
// states, then schedules FUN_1804c7d80 (Sakuya descent / camera fade)
// which schedules FUN_1804ca430 (the actual Water Lily grant +
// FUN_1804ca860 tutorial loop).
//
// The skip-the-first-link approach: replace this entry with a stub
// that grants Water Lily so BrushMan fires the AP check, then jumps
// the stage descriptor to sub-state 0xe (the post-tutorial state
// FUN_1804ca860's success path transitions to). Everything in between
// is camera/animation/script and has no AP-side effect.
void __fastcall stubWaterLilyTutorial()
{
    eventfix::clearCutsceneModeBits();
    eventfix::grantBrush(okami::BrushOverlay::water_lily);
    eventfix::transitionStageSubState(0xe);
    wolf::logInfo("[eventfix] Kamiki Water Lily tutorial suppressed; granted Water Lily");
}

constexpr EventBypass kBypasses[] = {
    {"Kamiki Water Lily tutorial", 0x4C64A0, stubWaterLilyTutorial},
};

} // namespace

std::span<const EventBypass> getBypasses()
{
    return kBypasses;
}

} // namespace eventfix::kamiki_village
