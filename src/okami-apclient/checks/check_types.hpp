#pragma once

#include <cstdint>

/**
 * @brief Archipelago check (location) system types and utilities
 *
 * Checks are game events that trigger sending a "location check" to the
 * AP server. Each check has a unique ID computed from its category and
 * game-specific identifiers.
 *
 * ID Range Scheme: categories spaced 1e9 apart,
 * and 1000 slots per inner key (mapId / shopId / levelId).
 *
 * - 1_000_000_000 + brushIndex:                Brush acquisitions
 * - 2_000_000_000 + shopId*1000 + itemSlot:    Shop purchases
 * - 3_000_000_000 + mapId*1000 + bitIndex:     World state changes
 * - 4_000_000_000 + mapId*1000 + bitIndex:     Collected objects
 * - 5_000_000_000 + mapId*1000 + bitIndex:     Area restorations
 * - 6_000_000_000 + bitIndex:                  Global flags
 * - 7_000_000_000 + bitIndex:                  Game progress flags
 * - 8_000_000_000 + levelId*1000 + spawnIdx:   Container pickups
 *
 */
namespace checks
{

/**
 * @brief Convert WOLF monitor bit index to game BitField bit index
 *
 * The WOLF bitfield monitor uses LSB-0 ordering (bit 0 = least significant bit),
 * but the game's BitField struct uses MSB-0 ordering (bit 0 = most significant bit,
 * via mask 0x80000000 >> (index % 32)). This converts between the two conventions
 * within each 32-bit word.
 */
inline constexpr unsigned int monitorToGameBitIndex(unsigned int monitorBitIndex)
{
    unsigned int word = monitorBitIndex / 32;
    unsigned int bitInWord = monitorBitIndex % 32;
    return word * 32 + (31 - bitInWord);
}

// Check ID range bases. Categories sit 1e9 apart
inline constexpr int64_t kBrushAcquisitionBase = 1'000'000'000;
inline constexpr int64_t kShopPurchaseBase = 2'000'000'000;
inline constexpr int64_t kWorldStateBase = 3'000'000'000;
inline constexpr int64_t kCollectedObjectBase = 4'000'000'000;
inline constexpr int64_t kAreaRestoredBase = 5'000'000'000;
inline constexpr int64_t kGlobalFlagBase = 6'000'000'000;
inline constexpr int64_t kGameProgressBase = 7'000'000'000;
inline constexpr int64_t kContainerBase = 8'000'000'000;

/**
 * @brief Calculate check ID for brush acquisition
 * @param brushIndex The game's internal brush index
 * @return Archipelago check ID (kBrushAcquisitionBase + brushIndex)
 */
inline constexpr int64_t getBrushCheckId(int brushIndex)
{
    return kBrushAcquisitionBase + brushIndex;
}

/**
 * @brief Calculate check ID for shop purchase
 * @param shopId The game's internal shop ID
 * @param itemSlot The slot number within the shop
 * @return Archipelago check ID (kShopPurchaseBase + shopId*1000 + itemSlot)
 */
inline constexpr int64_t getShopCheckId(int shopId, int itemSlot)
{
    return kShopPurchaseBase + (static_cast<int64_t>(shopId) * 1000) + itemSlot;
}

/**
 * @brief Calculate check ID for world state bit change
 * @param mapId The map where the bit change occurred
 * @param bitIndex The specific bit that changed
 * @return Archipelago check ID (kWorldStateBase + mapId*1000 + bitIndex)
 */
inline constexpr int64_t getWorldStateCheckId(int mapId, int bitIndex)
{
    return kWorldStateBase + (static_cast<int64_t>(mapId) * 1000) + bitIndex;
}

/**
 * @brief Calculate check ID for collected object
 * @param mapId The map where the object was collected
 * @param bitIndex The specific object bit that was set
 * @return Archipelago check ID (kCollectedObjectBase + mapId*1000 + bitIndex)
 */
inline constexpr int64_t getCollectedObjectCheckId(int mapId, int bitIndex)
{
    return kCollectedObjectBase + (static_cast<int64_t>(mapId) * 1000) + bitIndex;
}

/**
 * @brief Calculate check ID for area restoration
 * @param mapId The map where the area was restored
 * @param bitIndex The specific restoration bit that was set
 * @return Archipelago check ID (kAreaRestoredBase + mapId*1000 + bitIndex)
 */
inline constexpr int64_t getAreaRestoredCheckId(int mapId, int bitIndex)
{
    return kAreaRestoredBase + (static_cast<int64_t>(mapId) * 1000) + bitIndex;
}

/**
 * @brief Calculate check ID for global flag change
 * @param bitIndex The specific global flag bit that changed
 * @return Archipelago check ID (700000 + bitIndex)
 */
inline constexpr int64_t getGlobalFlagCheckId(int bitIndex)
{
    return kGlobalFlagBase + bitIndex;
}

/**
 * @brief Calculate check ID for game progress flag change
 * @param bitIndex The specific game progress bit that changed
 * @return Archipelago check ID (800000 + bitIndex)
 */
inline constexpr int64_t getGameProgressCheckId(int bitIndex)
{
    return kGameProgressBase + bitIndex;
}

/**
 * @brief Calculate check ID for container pickup
 * @param levelId The level ID where the container exists
 * @param spawnIdx The spawn table index of the container
 * @return Archipelago check ID (kContainerBase + levelId*1000 + spawnIdx)
 */
inline constexpr int64_t getContainerCheckId(uint16_t levelId, int spawnIdx)
{
    return kContainerBase + (static_cast<int64_t>(levelId) * 1000) + spawnIdx;
}

/**
 * @brief Determine which category a check ID belongs to
 */
enum class CheckCategory
{
    BrushAcquisition,
    ShopPurchase,
    WorldState,
    CollectedObject,
    AreaRestored,
    GlobalFlag,
    GameProgress,
    Container,
    Unknown
};

/**
 * @brief Get the category for a check ID
 */
inline constexpr CheckCategory getCheckCategory(int64_t checkId)
{
    if (checkId >= kContainerBase)
        return CheckCategory::Container;
    if (checkId >= kGameProgressBase)
        return CheckCategory::GameProgress;
    if (checkId >= kGlobalFlagBase)
        return CheckCategory::GlobalFlag;
    if (checkId >= kAreaRestoredBase)
        return CheckCategory::AreaRestored;
    if (checkId >= kCollectedObjectBase)
        return CheckCategory::CollectedObject;
    if (checkId >= kWorldStateBase)
        return CheckCategory::WorldState;
    if (checkId >= kShopPurchaseBase)
        return CheckCategory::ShopPurchase;
    if (checkId >= kBrushAcquisitionBase)
        return CheckCategory::BrushAcquisition;
    return CheckCategory::Unknown;
}

} // namespace checks
