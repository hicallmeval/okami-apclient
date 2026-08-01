#pragma once

#include <cstdint>
#include <expected>

#include "reward_types.hpp"

namespace rewards::traps
{

inline constexpr int64_t kTrapDamage   = 0x400; // Evil Charm:    deal 50% max HP (never lethal)
inline constexpr int64_t kTrapInkLoss  = 0x401; // Dry Inkwell:   drain all current ink
inline constexpr int64_t kTrapFoodLoss = 0x402; // Hungry Spirit: drain the Astral Pouch (food)

[[nodiscard]] std::expected<void, RewardError> grant(int64_t apItemId);

} // namespace rewards::traps
