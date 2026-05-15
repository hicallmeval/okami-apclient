#include "common.hpp"

#include <wolf_framework.hpp>

namespace eventfix
{

namespace
{

// Module-relative offsets (image base 0x180000000 in Ghidra).
constexpr uintptr_t kOff_AreaLoadFlags = 0xB6B2A0;
constexpr uintptr_t kOff_GlobalStateWord0 = 0xB6B2AC;
constexpr uint32_t kCutsceneModeBitsAreaFlags = 0x02802000;
constexpr uint32_t kCutsceneModeBitsW0 = 0x40000040;

constexpr uintptr_t kOff_BrushBitOp = 0x17C270;              // FUN_18017c270
constexpr uintptr_t kOff_BrushState = 0x8909C0;              // DAT_1808909c0
constexpr uintptr_t kOff_StateBitSet = 0x170830;             // FUN_180170830
constexpr uintptr_t kOff_StateBitClear = 0x1707C0;           // FUN_1801707c0
constexpr uintptr_t kOff_StageSubStateTransition = 0x48C720; // FUN_18048c720
constexpr uintptr_t kOff_StageDescriptor = 0xB65ED0;         // DAT_180b65ed0

using BrushBitOpFn = void(__fastcall *)(void *brushState, uint32_t bitIdx, int32_t op);
using StateBitFn = void(__fastcall *)(uint32_t encoded);
using SubStateFn = void(__fastcall *)(void *descriptor, uint32_t subStateId);

uintptr_t s_mainBase = 0;

} // namespace

bool initializeCommon()
{
    if (s_mainBase != 0)
        return true;
    s_mainBase = wolf::getModuleBase("main.dll");
    return s_mainBase != 0;
}

void clearCutsceneModeBits()
{
    if (s_mainBase == 0)
        return;
    auto *areaFlags = reinterpret_cast<volatile uint32_t *>(s_mainBase + kOff_AreaLoadFlags);
    *areaFlags &= ~kCutsceneModeBitsAreaFlags;
    auto *gameStateW0 = reinterpret_cast<volatile uint32_t *>(s_mainBase + kOff_GlobalStateWord0);
    *gameStateW0 &= ~kCutsceneModeBitsW0;
}

void grantBrush(okami::BrushOverlay brush)
{
    if (s_mainBase == 0)
        return;
    auto fn = reinterpret_cast<BrushBitOpFn>(s_mainBase + kOff_BrushBitOp);
    auto *brushState = reinterpret_cast<void *>(s_mainBase + kOff_BrushState);
    fn(brushState, static_cast<uint32_t>(brush), 0);
}

void setStateBit(uint32_t encoded)
{
    if (s_mainBase == 0)
        return;
    auto fn = reinterpret_cast<StateBitFn>(s_mainBase + kOff_StateBitSet);
    fn(encoded);
}

void clearStateBit(uint32_t encoded)
{
    if (s_mainBase == 0)
        return;
    auto fn = reinterpret_cast<StateBitFn>(s_mainBase + kOff_StateBitClear);
    fn(encoded);
}

void transitionStageSubState(uint32_t subStateId)
{
    if (s_mainBase == 0)
        return;
    auto fn = reinterpret_cast<SubStateFn>(s_mainBase + kOff_StageSubStateTransition);
    auto *descriptor = reinterpret_cast<void *>(s_mainBase + kOff_StageDescriptor);
    fn(descriptor, subStateId);
}

} // namespace eventfix
