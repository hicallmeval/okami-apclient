#pragma once

#include <cstdint>

#include <okami/brushes.hpp>

namespace eventfix
{

// Clears the cutscene-mode bits set by the dispatcher's pre-call to
// FUN_1803f4900. Mirrors the bits that function sets, NOT FUN_1803f4730
// (the natural exit) which clears slightly different masks.
//
//   AreaLoadFlags (main.dll+0xB6B2A0) &= ~0x02802000
//   GGS[0]        (main.dll+0xB6B2AC) &= ~0x40000040
//
// Call from a bypass stub to keep the camera in follow-Ammy mode and
// release the input/scene lock the dispatcher imposed before scheduling
// the suppressed callback.
void clearCutsceneModeBits();

// Sets the bit corresponding to `brush` in both the "usable"
// (brushState+0x70) and "obtained" (brushState+0x78) bitfields. Wraps
// FUN_18017c270 with op=0. BrushMan's watcher detects the transition
// and fires the AP check for that brush.
//
// Note: the brush bitfields use little-endian-within-64-bit-word
// encoding (1ULL << bit), the OPPOSITE of okami::BitField<N> and the
// state-bit-setter convention. See setStateBit() for the BE side.
void grantBrush(okami::BrushOverlay brush);

// Set/clear a bit in the world-state struct via FUN_180170830 /
// FUN_1801707c0 with the encoded address argument. Encoding:
//
//   addr = worldStateBase + (high16 * 0x20) + 0x35C + (low16/32) * 4
//   mask = 0x80000000 >> (low16 % 32)        // BIG-endian within word
//
// Convention is the opposite of grantBrush(). Use these for poking the
// engine's state-bit struct (gates, latches, etc.).
void setStateBit(uint32_t encoded);
void clearStateBit(uint32_t encoded);

// Drive the active stage descriptor (DAT_180b65ed0) into sub-state
// `subStateId` via FUN_18048c720(descriptor, subStateId). The stage
// descriptor governs per-stage callback installation and lifecycle;
// most natural tutorial chains end by transitioning to a specific
// sub-state in their epilogue. Call this from a bypass stub to land
// the descriptor in the same state the natural chain would have
// reached, without running the chain.
//
// No-op if initializeCommon() has not bound the main.dll base.
void transitionStageSubState(uint32_t subStateId);

// Initialize shared state. Called by eventfix::initialize() before any
// bypass is installed; binds the main.dll base address used by the
// helpers above. Returns false if main.dll isn't loaded.
bool initializeCommon();

} // namespace eventfix
