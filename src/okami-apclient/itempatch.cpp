#include "itempatch.hpp"

#include <algorithm>
#include <array>
#include <climits>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

#include <okami/msd.h>

#include <okami/bitfield.hpp>
#include <okami/customiconpkg.hpp>
#include <okami/custommodelpkg.hpp>
#include <okami/offsets.hpp>
#include <okami/structs.hpp>
#include <wolf_framework.hpp>

#include "checks/check_types.hpp"
#include "gamestate_accessors.hpp"
#include "okami/itemtype.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace itempatch
{

using LoadRscIdxFn = void *(__fastcall *)(void *pPkg, uint32_t idx);
using LoadRscPkgAsyncFn = void *(__fastcall *)(void *pFs, const char *pszFilename, void **pOutputRscData, void *pHeap, int32_t, int32_t, int32_t, int32_t);
using GetNumEntriesFn = int64_t(__fastcall *)(void *pTextureManager, int32_t texGroup);
using GetItemIconFn = hx::Texture *(__fastcall *)(okami::cItemShop * pShop, int item);
using LoadCore20MSDFn = void(__fastcall *)(void *pMsgStruct);
using GetMSDStringFn = const uint16_t *(__fastcall *)(void *pBase, uint16_t index);
using BuildSlotArrayFn = uint32_t(__fastcall *)(okami::cItemShop *pShop);
using BuildItemResourceNameFn = void(__fastcall *)(void *, int, uint32_t, char *);
using LoadItemModelFn = uint32_t(__fastcall *)(void *entity);

constexpr int64_t kItemIconTextureSlots = 300; // Originally 128; expanded to fit all NUM_ITEM_TYPES icons

static LoadRscIdxFn s_loadRscIdx = nullptr;
static LoadRscPkgAsyncFn s_origLoadRscPkgAsync = nullptr;
static GetNumEntriesFn s_origGetNumEntries = nullptr;
static GetItemIconFn s_origGetItemIcon = nullptr;
static LoadCore20MSDFn s_origLoadCore20MSD = nullptr;
static GetMSDStringFn s_origGetMSDString = nullptr;
static BuildSlotArrayFn s_origBuildSlotArray = nullptr;
static BuildItemResourceNameFn s_origBuildItemResourceName = nullptr;
static LoadItemModelFn s_origLoadItemModel = nullptr;

static void **s_ppCore20MSD = nullptr;
static okami::MSDManager s_msdManager;

// 0-based index of the first custom entry in the combined icon package.
// -1 means the combined package was not built (icons will fall through to vanilla).
static int s_customIconBase = -1;

constexpr int16_t kCustomStringBase = 0x1000;
// Key: virtual index. Value: encoded UTF-16 string.
// std::unordered_map guarantees reference stability on insert — data() pointers remain valid.
static std::unordered_map<int16_t, std::vector<uint16_t>> s_customStrings;
static std::unordered_map<int64_t, int16_t> s_locationIndex;
static int16_t s_nextCustomIndex = kCustomStringBase;
static int s_currentShopId = -1;
static int64_t s_currentContainerLocation = -1;
static void *s_pCurrentShop = nullptr;

// -1 = read live game state; 0/1 = forced for tests
static int s_shopMenuActiveOverride = -1;

constexpr unsigned kItemShopMenuBit = 10; // GlobalGameState bit; flips 0->1 on shop entry, 1->0 on exit

/// Returns true when the game's "Item shop menu" GlobalGameState bit is set.
/// MSD substitutions in resolveApItemName and hookGetMSDString must be gated
/// by this so name swaps don't leak into cutscenes, dialog boxes, brush
/// trigger overlays, or anywhere else outside the active shop UI.
static bool isShopMenuActive()
{
    if (s_shopMenuActiveOverride >= 0)
        return s_shopMenuActiveOverride != 0;

    const auto mainBase = reinterpret_cast<uintptr_t>(wolf::getModuleBase("main.dll"));
    if (!mainBase)
        return false;
    const auto *bits = reinterpret_cast<const okami::BitField<86> *>(mainBase + okami::main::globalGameStateFlags);
    return bits->IsSet(kItemShopMenuBit);
}

/// Returns the directory containing apclient.dll at runtime.
/// Falls back to "mods/apclient" (CWD-relative) on non-Windows builds.
static std::filesystem::path getModDir()
{
#ifdef _WIN32
    HMODULE hMod = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&getModDir), &hMod);
    wchar_t buf[MAX_PATH]{};
    GetModuleFileNameW(hMod, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
#else
    return std::filesystem::path("mods/apclient");
#endif
}

static int64_t __fastcall hookGetNumEntries(void *pMgr, int32_t texGroup)
{
    if (texGroup == 4)
        return kItemIconTextureSlots;
    return s_origGetNumEntries(pMgr, texGroup);
}

static void *__fastcall hookLoadRscPkgAsync(void *pFs, const char *pszFilename, void **pOutputRscData, void *pHeap, int32_t a5, int32_t a6, int32_t a7,
                                            int32_t a8)
{
    // Redirect the vanilla icon package to our combined package (single async load —
    // adding a second pending load hangs the game indefinitely).
    if (pszFilename && std::strcmp(pszFilename, "id/ItemShopBuyIcon.dat") == 0 && s_customIconBase >= 0)
    {
        wolf::logDebug("[itempatch] Icon package redirected: id/ItemShopBuyIcon.dat -> %s", okami::customiconpkg::kPackageResourceName.data());
        return s_origLoadRscPkgAsync(pFs, okami::customiconpkg::kPackageResourceName.data(), pOutputRscData, pHeap, a5, a6, a7, a8);
    }
    return s_origLoadRscPkgAsync(pFs, pszFilename, pOutputRscData, pHeap, a5, a6, a7, a8);
}

static hx::Texture *__fastcall hookGetItemIcon(okami::cItemShop *pShop, int item)
{
    // When the combined package is loaded, pShop->pIconsRsc IS the combined package.
    // Custom entries occupy indices s_customIconBase..s_customIconBase+kIconEntries.size()-1.
    if (s_customIconBase >= 0 && pShop->pIconsRsc)
    {
        for (int i = 0; i < static_cast<int>(okami::customiconpkg::kIconEntries.size()); ++i)
        {
            if (okami::customiconpkg::kIconEntries[static_cast<size_t>(i)].itemType == item)
            {
                auto *tex = static_cast<hx::Texture *>(s_loadRscIdx(pShop->pIconsRsc, static_cast<uint32_t>(s_customIconBase + i)));
                if (tex)
                    return tex;
                break; // entry found but texture not ready; fall through to vanilla
            }
        }
    }

    // Vanilla lookup: the game uses internal lookup tables to map
    // item types to package indices. Delegate to the original function.
    return s_origGetItemIcon(pShop, item);
}

// Ground truth from in-game testing (see issue #124 reproduction screenshots):
//   * The shop's row LIST queries `294 + itemType` -- the standard item-name
//     base. Every visible row of the same dummy type shares one strId, so a
//     slot-keyed substitution there would make every row of a type render as
//     the currently-selected slot's name and flicker on cursor movement.
//   * The shop's bottom INFO PANEL queries `0x2000 + itemType`. Only one item
//     is shown there at a time (the selected slot), so resolving to the
//     selected slot's scouted name is safe and unambiguous.
constexpr uint16_t kListBase = 294;
constexpr uint16_t kInfoPanelBase = 0x2000;

constexpr auto kDummyTypes = std::to_array<uint16_t>({
    okami::ItemTypes::ForeignStandardItem,
    okami::ItemTypes::ForeignProgressionItem,
    okami::ItemTypes::ForeignTrapItem,
    okami::ItemTypes::OkamiStandardItem,
    okami::ItemTypes::OkamiProgressionItem,
    okami::ItemTypes::OkamiTrapItem,
});

/// True for an AP dummy item's info-panel strId (itemType + 0x2000). Only the
/// selected slot's item is rendered through this strId, so it's safe to
/// substitute the scouted name here.
static constexpr bool isApDummyInfoPanelStrId(uint16_t index)
{
    return std::ranges::any_of(kDummyTypes, [&](uint16_t t) { return index == t + kInfoPanelBase; });
}

/// True for an AP dummy item's shop-list strId (itemType + 294). Multiple list
/// rows share the same strId per type, so a slot-keyed lookup would make every
/// visible row render as the currently-selected slot's name (issue #124).
static constexpr bool isApDummyListStrId(uint16_t index)
{
    return std::ranges::any_of(kDummyTypes, [&](uint16_t t) { return index == t + kListBase; });
}

/// True for either path. Used by the container floating-name path, where the
/// strId may come through either channel and there is only one item visible.
static constexpr bool isApDummyAnyStrId(uint16_t index)
{
    return isApDummyInfoPanelStrId(index) || isApDummyListStrId(index);
}

static void __fastcall hookLoadCore20MSD(void *pMsgStruct)
{
    s_origLoadCore20MSD(pMsgStruct);

    if (!s_ppCore20MSD || !*s_ppCore20MSD)
    {
        wolf::logWarning("[itempatch] hookLoadCore20MSD: ppCore20MSD is null, skipping patch");
        return;
    }

    s_msdManager = okami::MSDManager{};
    s_msdManager.ReadMSD(*s_ppCore20MSD);

    // Override strings for items missing from vanilla MSD
    constexpr uint32_t ItemStrBaseID = 294; // magic
    s_msdManager.OverrideString(okami::ItemTypes::HourglassOrb + ItemStrBaseID, "Hourglass Orb");
    s_msdManager.OverrideString(okami::ItemTypes::Yen10 + ItemStrBaseID, "10 Yen");
    s_msdManager.OverrideString(okami::ItemTypes::Yen50 + ItemStrBaseID, "50 Yen");
    s_msdManager.OverrideString(okami::ItemTypes::Yen100 + ItemStrBaseID, "100 Yen");
    s_msdManager.OverrideString(okami::ItemTypes::Yen150 + ItemStrBaseID, "150 Yen");
    s_msdManager.OverrideString(okami::ItemTypes::Yen500 + ItemStrBaseID, "500 Yen");
    s_msdManager.OverrideString(okami::ItemTypes::Praise + ItemStrBaseID, "Praise");

    // AP dummy item names — Foreign types (other player's items)
    s_msdManager.OverrideString(okami::ItemTypes::ForeignStandardItem + ItemStrBaseID, "Archipelago Item");
    s_msdManager.OverrideString(okami::ItemTypes::ForeignProgressionItem + ItemStrBaseID, "Archipelago Progression");
    s_msdManager.OverrideString(okami::ItemTypes::ForeignTrapItem + ItemStrBaseID, "Archipelago Trap");

    // Okami-native dummy item names (local player's non-displayable items: brushes, weapons, etc.)
    s_msdManager.OverrideString(okami::ItemTypes::OkamiStandardItem + ItemStrBaseID, "Okami Item");
    s_msdManager.OverrideString(okami::ItemTypes::OkamiProgressionItem + ItemStrBaseID, "Okami Progression");
    s_msdManager.OverrideString(okami::ItemTypes::OkamiTrapItem + ItemStrBaseID, "Okami Trap");

    // Redirect MSD data pointer to our patched version
    *s_ppCore20MSD = const_cast<uint8_t *>(s_msdManager.GetData());

    wolf::logInfo("[itempatch] Core20MSD patched: %zu strings", s_msdManager.Size());
}

const uint16_t *resolveApItemName(uint16_t strId)
{
    // Container path: only one floating item is visible at a time, so a single
    // location->name lookup is unambiguous. Either strId path may fire here.
    if (s_currentContainerLocation >= 0 && isApDummyAnyStrId(strId))
    {
        auto locIt = s_locationIndex.find(s_currentContainerLocation);
        if (locIt != s_locationIndex.end())
        {
            auto strIt = s_customStrings.find(locIt->second);
            if (strIt != s_customStrings.end())
                return strIt->second.data();
        }
    }

    // Shop path: only resolve while the player is actually looking at a shop
    // menu. Without this gate, stale shop context bleeds the scouted name into
    // cutscene area titles, brush textboxes, etc. (issue #113). And only the
    // info-panel strId (0x2000+) resolves to the selected slot -- the list
    // strId (294+) is shared across every visible row of the same dummy type,
    // so resolving by selected slot would make all those row names flicker as
    // the cursor moves between them (issue #124). The list path falls through
    // to the override we set during hookLoadCore20MSD, which yields the dummy
    // type's generic name ("Archipelago Item", "Okami Progression", etc.).
    if (s_currentShopId >= 0 && s_pCurrentShop && isShopMenuActive() && isApDummyInfoPanelStrId(strId))
    {
        // Offsets validated against decompiled FUN_18043ca30 (cItemShop::PurchaseItem)
        auto *shopBase = reinterpret_cast<uint8_t *>(s_pCurrentShop);
        int selectedSlot = shopBase[0x8A] + shopBase[0x8B];
        int64_t locationId = checks::getShopCheckId(s_currentShopId, selectedSlot);
        auto locIt = s_locationIndex.find(locationId);
        if (locIt != s_locationIndex.end())
        {
            auto strIt = s_customStrings.find(locIt->second);
            if (strIt != s_customStrings.end())
                return strIt->second.data();
        }
    }
    return nullptr;
}

static const uint16_t *__fastcall hookGetMSDString(void *pBase, uint16_t index)
{
    const uint16_t *apResult = resolveApItemName(index);
    if (apResult)
        return apResult;

    // The shop's bottom info-panel queries `0x2000 + itemType`, but items not
    // normally sold in shops (HourglassOrb, Yen, Praise, our dummy types, etc.)
    // have no entry at that strId. Fall back to the standard item-name strId
    // (294 + itemType) where our hookLoadCore20MSD overrides live. Gated by the
    // live "Item shop menu" GlobalGameState bit so the remap doesn't fire in
    // cutscenes, where strIds in the 0x2000+ range may belong to unrelated
    // dialog and would otherwise be silently rewritten (issue #113).
    if (isShopMenuActive() && index >= kInfoPanelBase && index < kInfoPanelBase + okami::ItemTypes::NUM_ITEM_TYPES)
    {
        uint16_t remapped = static_cast<uint16_t>((index - kInfoPanelBase) + kListBase);
        return s_origGetMSDString(pBase, remapped);
    }

    // No more intercepts: every other path -- cutscene area banners, brush
    // textboxes, the bottom-of-screen dialog system, etc. -- must pass through
    // unchanged. (We previously kept a fallback here that looked up indices
    // >= kCustomStringBase in s_customStrings, but the game allocates strIds
    // throughout the 0x1000+ range and would silently match our virtual
    // indices, replacing area names with item names. See issue #113.)
    return s_origGetMSDString(pBase, index);
}

static void __fastcall hookBuildItemResourceName(void *param_1, int item_type, uint32_t item_id, char *output)
{
    // If item_id matches an AP dummy type, redirect to chestnut (0x83) to avoid
    // a crash from loading a non-existent resource file.
    constexpr uint8_t kDummyIds[] = {
        static_cast<uint8_t>(okami::ItemTypes::ForeignStandardItem),    // 0x78
        static_cast<uint8_t>(okami::ItemTypes::ForeignProgressionItem), // 0x82
        static_cast<uint8_t>(okami::ItemTypes::ForeignTrapItem),        // 0xAF
        static_cast<uint8_t>(okami::ItemTypes::OkamiStandardItem),      // 0xA2
        static_cast<uint8_t>(okami::ItemTypes::OkamiProgressionItem),   // 0xA8
        static_cast<uint8_t>(okami::ItemTypes::OkamiTrapItem),          // 0xAC
    };

    for (uint8_t dummyId : kDummyIds)
    {
        if (static_cast<uint8_t>(item_id) == dummyId)
        {
            // Redirect to chestnut model (0x83) to avoid missing resource crash
            s_origBuildItemResourceName(param_1, item_type, 0x83, output);
            return;
        }
    }

    s_origBuildItemResourceName(param_1, item_type, item_id, output);
}

/// Hook for FUN_18020e750 (item model loader, called from cItemObj::init).
/// Swaps entity+0xe80 (resource package pointer) to our custom box model for AP dummy entities.
static uint32_t __fastcall hookLoadItemModel(void *entity)
{
    auto *e = static_cast<uint8_t *>(entity);
    uint32_t entityType = *reinterpret_cast<uint32_t *>(e + 0xe88);

    if (okami::custommodelpkg::isApDummyEntity(entityType))
    {
        void *pkg = okami::custommodelpkg::getPackageForEntity(entityType);
        if (pkg)
        {
            *reinterpret_cast<void **>(e + 0xe80) = pkg;
            wolf::logDebug("[itempatch] Swapped model package for entity type 0x%04X", entityType);
        }
    }
    return s_origLoadItemModel(entity);
}

static uint32_t __fastcall hookBuildSlotArray(okami::cItemShop *pShop)
{
    uint32_t result = s_origBuildSlotArray(pShop);
    s_pCurrentShop = pShop;
    return result;
}

void setCurrentShopId(int shopId)
{
    s_currentShopId = shopId;
}

void setShopPointer(void *pShop)
{
    s_pCurrentShop = pShop;
}

void clearShopContext()
{
    s_currentShopId = -1;
    s_pCurrentShop = nullptr;
}

void setContainerContext(int64_t locationId)
{
    s_currentContainerLocation = locationId;
}

void clearContainerContext()
{
    s_currentContainerLocation = -1;
}

void resetState()
{
    s_customStrings.clear();
    s_locationIndex.clear();
    s_nextCustomIndex = kCustomStringBase;
    clearShopContext();
    clearContainerContext();
    s_shopMenuActiveOverride = -1;
}

void setShopMenuActiveOverrideForTests(int state)
{
    s_shopMenuActiveOverride = state < 0 ? -1 : (state ? 1 : 0);
}

void setOrigGetMSDStringForTests(GetMSDStringForTestsFn fn)
{
    s_origGetMSDString = reinterpret_cast<GetMSDStringFn>(fn);
}

bool hasCustomIcons()
{
    return s_customIconBase >= 0;
}

void setHasCustomIconsForTests(bool present)
{
    s_customIconBase = present ? 1 : -1;
}

void registerScoutedItemName(int64_t locationId, const std::string &name)
{
    if (s_nextCustomIndex >= INT16_MAX)
    {
        wolf::logError("[itempatch] Custom string index overflow — too many scouted items registered");
        return;
    }

    if (s_locationIndex.contains(locationId))
        return; // idempotent

    int16_t idx = s_nextCustomIndex++;
    s_customStrings[idx] = okami::MSDManager::CompileString(name);
    s_locationIndex[locationId] = idx;
    wolf::logDebug("[itempatch] Registered '%s' for loc %lld -> virtual idx 0x%04X", name.c_str(), locationId, static_cast<uint16_t>(idx));
}

void initializeEarly()
{
    // Build the combined icon package (vanilla + custom AP icons) into
    // data_pc/archipelago/ before flower_startup scans the virtual FS tree.
    // The resource name "archipelago/customicons.dat" must exist on disk under
    // data_pc/ at tree-scan time so that LoadContextBuild can resolve it; writing
    // here (EarlyGameInit, before flower_startup) guarantees that ordering.
    const auto modDir = getModDir();
    const auto standardPath = modDir / "game-data/icons/ap_standard.dds";
    const auto progressionPath = modDir / "game-data/icons/ap_progression.dds";
    const auto trapPath = modDir / "game-data/icons/ap_trap.dds";

    const auto cwdPkgPath = std::filesystem::current_path() / "data_pc" / "archipelago" / "customicons.dat";

    auto vanillaPath = std::filesystem::current_path() / "id" / "ItemShopBuyIcon.dat";
    if (!std::filesystem::exists(vanillaPath))
        vanillaPath = std::filesystem::current_path() / "data_pc" / "id" / "ItemShopBuyIcon.dat";

    auto buildResult = okami::customiconpkg::buildCombinedFromFiles(cwdPkgPath, vanillaPath, standardPath, progressionPath, trapPath);
    if (buildResult)
    {
        s_customIconBase = *buildResult + 1; // +1: s_loadRscIdx is 1-based
        wolf::logInfo("[itempatch] Combined icon package built: %s (vanilla: %d, custom: %zu)", cwdPkgPath.string().c_str(), *buildResult,
                      okami::customiconpkg::kIconEntries.size());
    }
    else
    {
        // Shop slots backed by AP-dummy item types will fall back to a vanilla
        // item to avoid the icon-table OOB crash; see ShopMan::populateShopFromScoutedData.
        wolf::logError("[itempatch] Failed to build combined icon package: %s", buildResult.error().c_str());
    }

    // GetNumEntries must be hooked before flower_startup runs the texture
    // manager initialiser (FUN_1804B6850 -> FUN_180141460), which calls
    // GetNumEntries(4) to determine how many hx::Texture slots to allocate for
    // the item-icon group.  Installing the hook here (EarlyGameInit) ensures the
    // 300-slot array is allocated once, matching every later call to the hook.
    if (!wolf::hookFunction("main.dll", 0x1412B0, reinterpret_cast<void *>(&hookGetNumEntries), reinterpret_cast<void **>(&s_origGetNumEntries)))
        wolf::logError("[itempatch] Failed to install GetNumEntries hook");
}

void initialize()
{
    uintptr_t mainBase = reinterpret_cast<uintptr_t>(wolf::getModuleBase("main.dll"));

    // Resolve function pointer (not hooked, just called directly)
    s_loadRscIdx = reinterpret_cast<LoadRscIdxFn>(mainBase + 0x1B16C0);

    // Resolve ppCore20MSD pointer-to-pointer
    s_ppCore20MSD = reinterpret_cast<void **>(mainBase + 0x9C11B0);

    // Build in-memory colored box model packages for AP dummy items
    okami::custommodelpkg::initialize();

    // Patch the item flags table so AP dummy items are collectible.
    // Table at main.dll+0x7ab224: uint32[itemId*3] = flags stored to entity+0x11ac.
    // Bit 0x01: enables pickup animation setup (sets entity+0x1120=1)
    // Bit 0x40: enables proximity-based pickup check in FUN_180497bc0
    {
        constexpr uintptr_t kItemFlagsTableOffset = 0x7ab224;
        constexpr uint32_t kCollectableFlags = 0x41; // bit 0 + bit 6
        constexpr uint8_t kDummyItemIds[] = {
            static_cast<uint8_t>(okami::ItemTypes::ForeignStandardItem),  static_cast<uint8_t>(okami::ItemTypes::ForeignProgressionItem),
            static_cast<uint8_t>(okami::ItemTypes::ForeignTrapItem),      static_cast<uint8_t>(okami::ItemTypes::OkamiStandardItem),
            static_cast<uint8_t>(okami::ItemTypes::OkamiProgressionItem), static_cast<uint8_t>(okami::ItemTypes::OkamiTrapItem),
        };

        for (uint8_t id : kDummyItemIds)
        {
            // Each table entry = 3 × uint32_t (12 bytes); flags is the first uint32.
            uintptr_t flagsAddr = mainBase + kItemFlagsTableOffset + static_cast<uintptr_t>(id) * 12;
            if (wolf::writeMemory(flagsAddr, &kCollectableFlags, sizeof(kCollectableFlags)))
                wolf::logDebug("[itempatch] Patched item flags for ID 0x%02X at 0x%zX", id, flagsAddr);
            else
                wolf::logError("[itempatch] Failed to patch item flags for ID 0x%02X", id);
        }
    }

    if (!wolf::hookFunction("main.dll", 0x1AFC90, reinterpret_cast<void *>(&hookLoadRscPkgAsync), reinterpret_cast<void **>(&s_origLoadRscPkgAsync)))
        wolf::logError("[itempatch] Failed to install LoadRscPkgAsync hook");

    if (!wolf::hookFunction("main.dll", 0x43BDA0, reinterpret_cast<void *>(&hookGetItemIcon), reinterpret_cast<void **>(&s_origGetItemIcon)))
        wolf::logError("[itempatch] Failed to install GetItemIcon hook");

    if (!wolf::hookFunction("main.dll", 0x1C9510, reinterpret_cast<void *>(&hookLoadCore20MSD), reinterpret_cast<void **>(&s_origLoadCore20MSD)))
        wolf::logError("[itempatch] Failed to install LoadCore20MSD hook");

    if (!wolf::hookFunction("main.dll", 0x1C8A80, reinterpret_cast<void *>(&hookGetMSDString), reinterpret_cast<void **>(&s_origGetMSDString)))
        wolf::logError("[itempatch] Failed to install GetMSDString hook");

    if (!wolf::hookFunction("main.dll", 0x43E250, reinterpret_cast<void *>(&hookBuildSlotArray), reinterpret_cast<void **>(&s_origBuildSlotArray)))
        wolf::logError("[itempatch] Failed to install BuildSlotArray hook");

    if (!wolf::hookFunction("main.dll", 0x450b90, reinterpret_cast<void *>(&hookBuildItemResourceName),
                            reinterpret_cast<void **>(&s_origBuildItemResourceName)))
        wolf::logError("[itempatch] Failed to install BuildItemResourceName hook");

    if (!wolf::hookFunction("main.dll", 0x20e750, reinterpret_cast<void *>(&hookLoadItemModel), reinterpret_cast<void **>(&s_origLoadItemModel)))
        wolf::logError("[itempatch] Failed to install LoadItemModel hook");

    wolf::logInfo("[itempatch] Hooks installed");
}

void patchItemParams()
{
    auto &params = *apgame::itemParams;

    for (int i = 0; i < okami::ItemTypes::NUM_ITEM_TYPES; ++i)
    {
        auto &p = params.at(static_cast<size_t>(i));
        if (p.category == 0)
            p.category = 1;
        if (p.maxCount == 0)
            p.maxCount = 99;
    }

    // AP dummy item types: distinct categories for icon colors, max 1 per shop slot
    constexpr auto kDummyItems = std::to_array({
        okami::ItemTypes::ForeignStandardItem,
        okami::ItemTypes::ForeignProgressionItem,
        okami::ItemTypes::ForeignTrapItem,
        okami::ItemTypes::OkamiStandardItem,
        okami::ItemTypes::OkamiProgressionItem,
        okami::ItemTypes::OkamiTrapItem,
    });
    constexpr uint8_t kDummyCategories[] = {1, 3, 4, 1, 3, 4};
    for (size_t i = 0; i < kDummyItems.size(); ++i)
    {
        params.at(kDummyItems[i]).category = kDummyCategories[i];
        params.at(kDummyItems[i]).maxCount = 1;
    }

    wolf::logInfo("[itempatch] ItemParam array patched (%d entries)", okami::ItemTypes::NUM_ITEM_TYPES);
}

} // namespace itempatch
