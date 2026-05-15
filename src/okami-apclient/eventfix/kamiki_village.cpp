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
//
// In-game testing revealed the minimal stub (clearCutsceneModeBits +
// grantBrush + transitionStageSubState) is insufficient: when the
// player reaches the trigger, prior cinematic state leaves the camera
// and input locked. The natural FUN_1804ca860 success path does more
// cleanup; we mirror the subset of it that controls camera/input:
//
//   * clear DAT_180b6b2a0 bit 0x80         (tutorial input-lock)
//   * clear DAT_180b6b2a4 bits 0x20020004  (secondary cinematic flags)
//   * call FUN_180170690(brushState)        (brush state release)
//   * call FUN_1803f48d0(stageCtx, 1)       (full cutscene-exit and
//                                            scheduler-frame release)
void __fastcall stubWaterLilyTutorial()
{
    eventfix::clearCutsceneModeBits();

    auto base = wolf::getModuleBase("main.dll");
    if (base != 0)
    {
        *reinterpret_cast<volatile uint32_t *>(base + 0xB6B2A0) &= ~0x80u;
        *reinterpret_cast<volatile uint32_t *>(base + 0xB6B2A4) &= ~0x20020004u;
    }

    eventfix::releaseBrushState();
    eventfix::grantBrush(okami::BrushOverlay::water_lily);
    eventfix::transitionStageSubState(0xe);
    eventfix::releaseSchedulerFrame();

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
