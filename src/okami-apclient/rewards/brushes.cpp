#include "brushes.hpp"

#include <array>
#include <cstdint>

#include <okami/brushes.hpp>
#include <wolf_framework.hpp>

#include "../gamestate_accessors.hpp"

namespace rewards::brushes
{

namespace
{

// Progressive brush upgrade bit indices (in brushUpgrades bitfield)
// Power Slash: base brush_index = 12 (BrushOverlay::power_slash)
// - Power Slash 2: bit 0
// - Power Slash 3: bit 10
constexpr std::array<uint32_t, 2> kPowerSlashUpgrades = {0, 10};

// Cherry Bomb: base brush_index = 25 (BrushOverlay::cherry_bomb)
// - Cherry Bomb 2: bit 6
// - Cherry Bomb 3: bit 11
constexpr std::array<uint32_t, 2> kCherryBombUpgrades = {6, 11};

// The brush bitfields the game's set-bit function manipulates (BrushData
// source + WorldStateData copies) are addressed with LSB-first-within-byte
// semantics: bit X means `bytes[X/8] & (1 << (X%8))`. These helpers preserve
// that convention.
//
// brushUpgrades (TrackerData) is different — the game queries it via the
// BitField<32> API (MSB-first within each 32-bit word), so its callers go
// through `apgame::brushUpgrades->IsSet/Set` directly, not these helpers.
template <typename Accessor> void setGameBit(Accessor &accessor, unsigned int bitIdx)
{
    auto *bytes = reinterpret_cast<volatile uint8_t *>(accessor.get_ptr());
    bytes[bitIdx / 8] |= static_cast<uint8_t>(1u << (bitIdx % 8));
}

template <typename Accessor> bool isGameBitSet(const Accessor &accessor, unsigned int bitIdx)
{
    const auto *bytes = reinterpret_cast<const volatile uint8_t *>(accessor.get_ptr());
    return (bytes[bitIdx / 8] & static_cast<uint8_t>(1u << (bitIdx % 8))) != 0;
}

/**
 * @brief Grant a simple (non-progressive) brush technique
 *
 * Writes to both the BrushData source and the WorldStateData copy. The game
 * propagates BrushData -> WorldStateData on sync, so writing only the copy
 * gets clobbered. BrushMan's monitor must be muted while this runs (see
 * RewardMan -> CheckMan::enableSending wiring).
 */
void grantSimpleBrush(int brushIndex)
{
    setGameBit(apgame::usableBrushesSource, static_cast<unsigned int>(brushIndex));
    setGameBit(apgame::obtainedBrushesSource, static_cast<unsigned int>(brushIndex));
    setGameBit(apgame::usableBrushTechniques, static_cast<unsigned int>(brushIndex));
    setGameBit(apgame::obtainedBrushTechniques, static_cast<unsigned int>(brushIndex));
}

/**
 * @brief Grant a progressive brush technique or upgrade
 *
 * @param brushIndex The base brush index
 * @param upgrades Array of upgrade bit indices
 * @return Success (always - no error cases)
 */
template <size_t N> std::expected<void, RewardError> grantProgressiveBrush(int brushIndex, const std::array<uint32_t, N> &upgrades)
{
    // Check if base brush is already obtained (source is authoritative)
    bool hasBase = isGameBitSet(apgame::obtainedBrushesSource, static_cast<unsigned int>(brushIndex));

    if (!hasBase)
    {
        grantSimpleBrush(brushIndex);
        return {};
    }

    // Find and grant next upgrade. brushUpgrades indices come from
    // okami::game_state::global::brushUpgrades and are BitField<32> indices
    // (MSB-first), so go through the BitField API directly.
    for (size_t i = 0; i < upgrades.size(); ++i)
    {
        uint32_t upgradeBit = upgrades[i];
        if (!apgame::brushUpgrades->IsSet(upgradeBit))
        {
            apgame::brushUpgrades->Set(upgradeBit);
            return {};
        }
    }

    return {}; // Already at max level
}

} // anonymous namespace

std::expected<void, RewardError> grant(int64_t apItemId)
{
    int brushIndex = getBrushIndex(apItemId);

    wolf::logDebug("Granting brush with AP ID 0x%X, brush index 0x%X", apItemId, brushIndex);

    if (apItemId == 0x10C) // Power Slash (bit 12)
    {
        return grantProgressiveBrush(brushIndex, kPowerSlashUpgrades);
    }
    if (apItemId == 0x119) // Cherry Bomb (bit 25)
    {
        return grantProgressiveBrush(brushIndex, kCherryBombUpgrades);
    }

    grantSimpleBrush(brushIndex);

    // Sunrise (bit 27) requires sunrise_default (bit 1) to actually be usable;
    // see okami::BrushOverlay. The Kamiki tutorial normally sets bit 1 when
    // the player first uses Sunrise, but if the AP item lands before that
    // tutorial fires we need to set bit 1 ourselves.
    if (apItemId == 0x11B) // Sunrise
    {
        grantSimpleBrush(static_cast<int>(okami::BrushOverlay::sunrise_default));
    }

    // Bloom (bit 4) is the bloom-circle ability. The two adjacent bits —
    // greensprout (2) and dot_trees (3) — gate cursed-patch bloom and
    // tree-growing respectively, and the apworld doesn't ship them as
    // separate items. Vanilla Sakigami sets all three at once; mirror that.
    if (apItemId == 0x104) // Greensprout (Bloom)
    {
        grantSimpleBrush(static_cast<int>(okami::BrushOverlay::greensprout));
        grantSimpleBrush(static_cast<int>(okami::BrushOverlay::dot_trees));
    }

    return {};
}

} // namespace rewards::brushes
