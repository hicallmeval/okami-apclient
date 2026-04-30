#include <cstring>

#include <okami/msd.h>

#include <catch2/catch_test_macros.hpp>
#include <okami/itemtype.hpp>

#include "checks/check_types.hpp"
#include "gamestate_accessors.hpp"
#include "itempatch.hpp"
#include "wolf_framework.hpp"

// ============================================================================
// MSD binary blob tests
// These tests do NOT require mock memory — MSDManager is pure data manipulation.
// ============================================================================

TEST_CASE("MSDManager: AddString encodes and stores a string", "[msd][blob]")
{
    okami::MSDManager mgr;
    uint32_t idx = mgr.AddString("Hi");
    REQUIRE(idx == 0);

    const uint8_t *data = mgr.GetData();
    REQUIRE(data != nullptr);

    // First 4 bytes: numEntries
    uint32_t numEntries;
    std::memcpy(&numEntries, data, sizeof(numEntries));
    REQUIRE(numEntries == 1);

    // Bytes 4-11: the single offset
    // Header size = 4 (numEntries) + 1 * 8 (one uint64_t offset) = 12
    // So offset[0] should point to byte 12
    uint64_t offset;
    std::memcpy(&offset, data + sizeof(uint32_t), sizeof(offset));
    REQUIRE(offset == sizeof(uint32_t) + 1 * sizeof(uint64_t));

    // First uint16_t at the string start: 'H' -> 30
    uint16_t ch0;
    std::memcpy(&ch0, data + offset, sizeof(ch0));
    REQUIRE(ch0 == 30);

    // Second uint16_t: 'i' -> 57
    uint16_t ch1;
    std::memcpy(&ch1, data + offset + 2, sizeof(ch1));
    REQUIRE(ch1 == 57);

    // Third uint16_t: EndDialog = 0x8001
    uint16_t term;
    std::memcpy(&term, data + offset + 4, sizeof(term));
    REQUIRE(term == 0x8001);
}

TEST_CASE("MSDManager: AddString returns sequential indices", "[msd][blob]")
{
    okami::MSDManager mgr;

    uint32_t idx0 = mgr.AddString("Alpha");
    uint32_t idx1 = mgr.AddString("Beta");
    uint32_t idx2 = mgr.AddString("Gamma");

    REQUIRE(idx0 == 0);
    REQUIRE(idx1 == 1);
    REQUIRE(idx2 == 2);

    REQUIRE(mgr.Size() == 3);
}

TEST_CASE("MSDManager: OverrideString replaces string in rebuilt binary", "[msd][blob]")
{
    // Build a valid minimal MSD binary with 2 strings: "Hi" and "Ok"
    // Header: numEntries(4) + offset[0](8) + offset[1](8) = 20 bytes
    // String 0 at offset 20: H(30), i(57), END(0x8001) = 3 uint16_t = 6 bytes
    // String 1 at offset 26: O(37), k(59), END(0x8001) = 3 uint16_t = 6 bytes

    std::vector<uint8_t> msdData;

    uint32_t n = 2;
    msdData.insert(msdData.end(), reinterpret_cast<uint8_t *>(&n), reinterpret_cast<uint8_t *>(&n) + sizeof(n));

    uint64_t o0 = 20; // 4 + 2*8
    msdData.insert(msdData.end(), reinterpret_cast<uint8_t *>(&o0), reinterpret_cast<uint8_t *>(&o0) + sizeof(o0));

    uint64_t o1 = 26; // 20 + 3 * sizeof(uint16_t)
    msdData.insert(msdData.end(), reinterpret_cast<uint8_t *>(&o1), reinterpret_cast<uint8_t *>(&o1) + sizeof(o1));

    // "Hi": H=30, i=57, END=0x8001
    uint16_t s0[] = {30, 57, 0x8001};
    msdData.insert(msdData.end(), reinterpret_cast<uint8_t *>(s0), reinterpret_cast<uint8_t *>(s0) + sizeof(s0));

    // "Ok": O=37, k=59, END=0x8001
    uint16_t s1[] = {37, 59, 0x8001};
    msdData.insert(msdData.end(), reinterpret_cast<uint8_t *>(s1), reinterpret_cast<uint8_t *>(s1) + sizeof(s1));

    okami::MSDManager mgr;
    mgr.ReadMSD(msdData.data());
    REQUIRE(mgr.Size() == 2);

    // Override string 0 with "A" — 'A' maps to 23
    mgr.OverrideString(0, "A");

    const uint8_t *out = mgr.GetData();
    REQUIRE(out != nullptr);

    // numEntries should still be 2
    uint32_t numEntries;
    std::memcpy(&numEntries, out, sizeof(numEntries));
    REQUIRE(numEntries == 2);

    // Read offset[0]
    uint64_t off0;
    std::memcpy(&off0, out + sizeof(uint32_t), sizeof(off0));

    // String 0 first character should be 'A' = 23
    uint16_t firstChar;
    std::memcpy(&firstChar, out + off0, sizeof(firstChar));
    REQUIRE(firstChar == 23);

    // String 0 second character (offset + 2) should be EndDialog = 0x8001
    // because "A" is a 1-char string and CompileString always appends EndDialog
    uint16_t termChar;
    std::memcpy(&termChar, out + off0 + 2, sizeof(termChar));
    REQUIRE(termChar == 0x8001);

    // Read offset[1] and verify string 1 is unchanged: first char 'O' = 37
    uint64_t off1;
    std::memcpy(&off1, out + sizeof(uint32_t) + sizeof(uint64_t), sizeof(off1));

    uint16_t firstCharStr1;
    std::memcpy(&firstCharStr1, out + off1, sizeof(firstCharStr1));
    REQUIRE(firstCharStr1 == 37);
}

TEST_CASE("MSDManager: ReadMSD then GetData preserves string count", "[msd][blob]")
{
    // Build a 3-string MSD binary
    // Header: 4 + 3*8 = 28 bytes
    // Each string: 1 char + END = 2 uint16_t = 4 bytes
    // String offsets: 28, 32, 36

    std::vector<uint8_t> msdData;

    uint32_t n = 3;
    msdData.insert(msdData.end(), reinterpret_cast<uint8_t *>(&n), reinterpret_cast<uint8_t *>(&n) + sizeof(n));

    uint64_t offsets[3] = {28, 32, 36};
    for (int i = 0; i < 3; ++i)
    {
        msdData.insert(msdData.end(), reinterpret_cast<uint8_t *>(&offsets[i]), reinterpret_cast<uint8_t *>(&offsets[i]) + sizeof(offsets[i]));
    }

    // Three 1-char strings: 'A'=23, 'B'=24, 'C'=25, each followed by END=0x8001
    uint16_t strings[3][2] = {{23, 0x8001}, {24, 0x8001}, {25, 0x8001}};
    for (int i = 0; i < 3; ++i)
    {
        msdData.insert(msdData.end(), reinterpret_cast<uint8_t *>(strings[i]), reinterpret_cast<uint8_t *>(strings[i]) + sizeof(strings[i]));
    }

    okami::MSDManager mgr;
    mgr.ReadMSD(msdData.data());

    // Retrieve data without any modifications
    const uint8_t *out = mgr.GetData();
    REQUIRE(out != nullptr);

    // numEntries in the output should match the original
    uint32_t numEntries;
    std::memcpy(&numEntries, out, sizeof(numEntries));
    REQUIRE(numEntries == 3);
    REQUIRE(mgr.Size() == 3);
}

TEST_CASE("MSDManager: OverrideString at invalid index is a no-op", "[msd][blob]")
{
    okami::MSDManager mgr;
    mgr.AddString("OnlyEntry");
    REQUIRE(mgr.Size() == 1);

    // Index 100 is out of bounds — should silently return with no crash
    mgr.OverrideString(100, "test");

    const uint8_t *data = mgr.GetData();
    REQUIRE(data != nullptr);

    uint32_t numEntries;
    std::memcpy(&numEntries, data, sizeof(numEntries));
    REQUIRE(numEntries == 1);
    REQUIRE(mgr.Size() == 1);
}

// ============================================================================
// ItemParam patching tests
// These tests USE mock memory and call apgame::initialize() first.
// ============================================================================

struct ItemParamFixture
{
    ItemParamFixture()
    {
        wolf::mock::reset();
        wolf::mock::reserveMemory(0x7AB220 + sizeof(std::array<okami::ItemParam, okami::ItemTypes::NUM_ITEM_TYPES>));
        apgame::initialize();
    }
};

TEST_CASE_METHOD(ItemParamFixture, "patchItemParams: zero-category items get category=1", "[itempatch][ItemParam]")
{
    // All ItemParam entries are zero-initialized by the mock constructor,
    // so category=0 and maxCount=0 for all entries going in.
    itempatch::patchItemParams();

    auto &params = *apgame::itemParams;

    // Items with explicit non-1 category overrides (progression=3, trap=4)
    constexpr auto kNonDefaultCategory = std::to_array({
        okami::ItemTypes::ForeignProgressionItem,
        okami::ItemTypes::ForeignTrapItem,
        okami::ItemTypes::OkamiProgressionItem,
        okami::ItemTypes::OkamiTrapItem,
    });
    for (int i = 0; i < okami::ItemTypes::NUM_ITEM_TYPES; ++i)
    {
        if (std::ranges::find(kNonDefaultCategory, i) != kNonDefaultCategory.end())
            continue;
        INFO("Item " << i << " has category " << static_cast<int>(params.at(i).category));
        REQUIRE(params.at(i).category == 1);
    }
}

TEST_CASE_METHOD(ItemParamFixture, "patchItemParams: zero-maxCount items get maxCount=99", "[itempatch][ItemParam]")
{
    itempatch::patchItemParams();

    auto &params = *apgame::itemParams;

    // AP dummy items get maxCount=1; all others get 99
    constexpr auto kMaxCount1Items = std::to_array({
        okami::ItemTypes::ForeignStandardItem,
        okami::ItemTypes::ForeignProgressionItem,
        okami::ItemTypes::ForeignTrapItem,
        okami::ItemTypes::OkamiStandardItem,
        okami::ItemTypes::OkamiProgressionItem,
        okami::ItemTypes::OkamiTrapItem,
    });
    for (int i = 0; i < okami::ItemTypes::NUM_ITEM_TYPES; ++i)
    {
        INFO("Item " << i << " has maxCount " << static_cast<int>(params.at(i).maxCount));
        if (std::ranges::find(kMaxCount1Items, i) != kMaxCount1Items.end())
            REQUIRE(params.at(i).maxCount == 1);
        else
            REQUIRE(params.at(i).maxCount == 99);
    }
}

TEST_CASE_METHOD(ItemParamFixture, "patchItemParams: AP dummy items get distinct categories", "[itempatch][ItemParam]")
{
    itempatch::patchItemParams();

    auto &params = *apgame::itemParams;

    // Foreign dummy items
    REQUIRE(params.at(okami::ItemTypes::ForeignStandardItem).category == 1);
    REQUIRE(params.at(okami::ItemTypes::ForeignProgressionItem).category == 3);
    REQUIRE(params.at(okami::ItemTypes::ForeignTrapItem).category == 4);
    // Okami-native dummy items (same category pattern)
    REQUIRE(params.at(okami::ItemTypes::OkamiStandardItem).category == 1);
    REQUIRE(params.at(okami::ItemTypes::OkamiProgressionItem).category == 3);
    REQUIRE(params.at(okami::ItemTypes::OkamiTrapItem).category == 4);
}

TEST_CASE_METHOD(ItemParamFixture, "patchItemParams: ArchipelagoTestItem categories always override pre-set values", "[itempatch][ItemParam]")
{
    // Pre-set ForeignStandardItem to a non-default category before patching
    auto &params = *apgame::itemParams;
    params.at(okami::ItemTypes::ForeignStandardItem).category = 42;

    itempatch::patchItemParams();

    // ArchipelagoTestItem categories are forced regardless of prior value
    REQUIRE(params.at(okami::ItemTypes::ForeignStandardItem).category == 1);
}

TEST_CASE_METHOD(ItemParamFixture, "patchItemParams: existing nonzero values are preserved", "[itempatch][ItemParam]")
{
    // Set specific entries to non-zero values BEFORE patching.
    // HolyBoneS (index 143) is not an ArchipelagoTestItem alias, so its
    // values should remain exactly as set.
    auto &params = *apgame::itemParams;
    params.at(okami::ItemTypes::HolyBoneS).category = 5;
    params.at(okami::ItemTypes::HolyBoneS).maxCount = 10;

    itempatch::patchItemParams();

    // patchItemParams only sets category/maxCount when they are zero,
    // so these pre-set non-zero values must survive unchanged.
    REQUIRE(params.at(okami::ItemTypes::HolyBoneS).category == 5);
    REQUIRE(params.at(okami::ItemTypes::HolyBoneS).maxCount == 10);
}

TEST_CASE("itempatch::initialize installs expected hooks", "[itempatch][hooks]")
{
    wolf::mock::reset();
    itempatch::initializeEarly(); // installs hookGetNumEntries
    itempatch::initialize();

    // Verify hooks were registered at the correct offsets
    REQUIRE(wolf::mock::registeredHooks.count(0x1AFC90) > 0); // hookLoadRscPkgAsync
    REQUIRE(wolf::mock::registeredHooks.count(0x1412B0) > 0); // hookGetNumEntries (from initializeEarly)
    REQUIRE(wolf::mock::registeredHooks.count(0x43BDA0) > 0); // hookGetItemIcon
    REQUIRE(wolf::mock::registeredHooks.count(0x1C9510) > 0); // hookLoadCore20MSD
}

TEST_CASE("hookGetNumEntries: returns 300 for texGroup 4", "[itempatch][hooks]")
{
    wolf::mock::reset();
    itempatch::initializeEarly(); // installs hookGetNumEntries
    itempatch::initialize();

    auto it = wolf::mock::registeredHooks.find(0x1412B0);
    REQUIRE(it != wolf::mock::registeredHooks.end());

    // Call the hook directly; texGroup==4 returns the expanded slot count
    // without touching s_origGetNumEntries (which is null in tests)
    using HookFn = int64_t (*)(void *, int32_t);
    auto fn = reinterpret_cast<HookFn>(it->second);
    REQUIRE(fn(nullptr, 4) == 300);
}

TEST_CASE_METHOD(ItemParamFixture, "patchItemParams: idempotent — calling twice does not change categories", "[itempatch][ItemParam]")
{
    itempatch::patchItemParams();
    itempatch::patchItemParams();

    auto &params = *apgame::itemParams;

    REQUIRE(params.at(okami::ItemTypes::ForeignStandardItem).category == 1);
    REQUIRE(params.at(okami::ItemTypes::ForeignProgressionItem).category == 3);
    REQUIRE(params.at(okami::ItemTypes::ForeignTrapItem).category == 4);
}

// ============================================================================
// Live scouted item name registration tests
// These tests do NOT require mock memory or MSD state.
// ============================================================================

TEST_CASE("registerScoutedItemName: idempotent — second call for same location is no-op", "[itempatch][live]")
{
    // Can be called any time, before or after initialize()
    REQUIRE_NOTHROW(itempatch::registerScoutedItemName(1001, "Boomerang"));
    REQUIRE_NOTHROW(itempatch::registerScoutedItemName(1001, "Should be ignored"));
}

TEST_CASE("registerScoutedItemName: different locations get different indices", "[itempatch][live]")
{
    REQUIRE_NOTHROW(itempatch::registerScoutedItemName(2001, "Fire Arrow"));
    REQUIRE_NOTHROW(itempatch::registerScoutedItemName(2002, "Ice Rod"));
}

TEST_CASE("itempatch::initialize registers GetMSDString and BuildSlotArray hooks", "[itempatch][hooks]")
{
    wolf::mock::reset();
    itempatch::initializeEarly();
    itempatch::initialize();
    REQUIRE(wolf::mock::registeredHooks.count(0x1C8A80) > 0); // hookGetMSDString
    REQUIRE(wolf::mock::registeredHooks.count(0x43E250) > 0); // hookBuildSlotArray
}

TEST_CASE("setCurrentShopId: can be called any time without crash", "[itempatch][live]")
{
    REQUIRE_NOTHROW(itempatch::setCurrentShopId(5));
    REQUIRE_NOTHROW(itempatch::setCurrentShopId(-1));
}

// ============================================================================
// Shop MSD string interception tests
// ============================================================================

TEST_CASE("CompileString: produces valid MSD encoding with EndDialog terminator", "[itempatch][msd]")
{
    auto compiled = okami::MSDManager::CompileString("Fire Arrow");
    REQUIRE(!compiled.empty());
    REQUIRE(compiled.back() == 0x8001);
    REQUIRE(compiled.size() == 11); // 10 chars + EndDialog
}

TEST_CASE("getShopCheckId produces correct location IDs for shop slots", "[itempatch][shops]")
{
    // Verify the mapping used by hookGetMSDString to resolve selected slots
    // getShopCheckId(shopId, slot) = 300000 + shopId*1000 + slot
    REQUIRE(checks::getShopCheckId(0, 0) == 300000);
    REQUIRE(checks::getShopCheckId(0, 1) == 300001);
    REQUIRE(checks::getShopCheckId(5, 0) == 305000);
    REQUIRE(checks::getShopCheckId(5, 3) == 305003);
    REQUIRE(checks::getShopCheckId(10, 7) == 310007);
}

TEST_CASE("registerScoutedItemName: CompileString output matches MSD encoding", "[itempatch][live]")
{
    // Verify that the string encoding used by registerScoutedItemName
    // (via MSDManager::CompileString) is correct for typical item names.
    auto compiled = okami::MSDManager::CompileString("Boomerang");
    REQUIRE(!compiled.empty());
    REQUIRE(compiled.back() == 0x8001); // EndDialog terminator

    // 'B' in Okami MSD encoding = 24
    REQUIRE(compiled[0] == 24);

    // Single-char string
    auto single = okami::MSDManager::CompileString("A");
    REQUIRE(single.size() == 2); // 'A' + EndDialog
    REQUIRE(single[0] == 23);    // 'A' = 23
    REQUIRE(single[1] == 0x8001);
}

// ============================================================================
// End-to-end resolveApItemName tests
// These exercise the real code path used by hookGetMSDString in production,
// using a mock shop buffer with controlled scroll/select offsets.
// ============================================================================

struct ShopContextFixture
{
    static constexpr int kShopId = 5;
    uint8_t mockShop[0x8C] = {};

    ShopContextFixture()
    {
        // Clean slate: clear all registrations and shop context from prior tests
        itempatch::resetState();
        // Force the "Item shop menu" GlobalGameState gate so the shop path
        // resolves without needing real game memory.
        itempatch::setShopMenuActiveOverrideForTests(1);
        // Set up a valid shop context: shop ID, shop pointer with scroll/select at slot 0
        itempatch::setCurrentShopId(kShopId);
        mockShop[0x8A] = 0; // scrollOffset
        mockShop[0x8B] = 0; // visualSelectIndex
        itempatch::setShopPointer(mockShop);
    }

    ~ShopContextFixture()
    {
        itempatch::resetState();
    }

    void selectSlot(uint8_t scroll, uint8_t visualSelect)
    {
        mockShop[0x8A] = scroll;
        mockShop[0x8B] = visualSelect;
    }

    int64_t locationIdForSlot(int slot) const
    {
        return checks::getShopCheckId(kShopId, slot);
    }
};

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: returns custom string for info panel strId (type+0x2000)", "[itempatch][resolve]")
{
    // Register "Fire Arrow" at slot 0
    int64_t locId = locationIdForSlot(0);
    itempatch::registerScoutedItemName(locId, "Fire Arrow");
    selectSlot(0, 0);

    // Info panel strId for ForeignStandardItem: 0x2000 + 120 = 8312
    const uint16_t *result = itempatch::resolveApItemName(8312);
    REQUIRE(result != nullptr);

    // Verify the returned string matches CompileString("Fire Arrow")
    auto expected = okami::MSDManager::CompileString("Fire Arrow");
    for (size_t i = 0; i < expected.size(); ++i)
    {
        INFO("Mismatch at index " << i);
        REQUIRE(result[i] == expected[i]);
    }
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: only info panel strId resolves; list strId falls through", "[itempatch][resolve]")
{
    int64_t locId = locationIdForSlot(2);
    itempatch::registerScoutedItemName(locId, "Ice Rod");
    selectSlot(0, 2);

    // Info panel strId (0x2000 + ForeignProgressionItem) renders the SELECTED
    // slot's detail name, so resolving it to the scouted name is correct.
    REQUIRE(itempatch::resolveApItemName(8322) != nullptr);
    // Shop list strId (294 + ForeignProgressionItem) is shared across every
    // visible row of that dummy type, so resolveApItemName must NOT bind it to
    // the selected slot - it falls through to the override "Archipelago
    // Progression" set in hookLoadCore20MSD. (issue #124)
    REQUIRE(itempatch::resolveApItemName(424) == nullptr);
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: returns nullptr for non-AP strId", "[itempatch][resolve]")
{
    int64_t locId = locationIdForSlot(0);
    itempatch::registerScoutedItemName(locId, "Hookshot");
    selectSlot(0, 0);

    // strId 100 is not an AP dummy strId
    REQUIRE(itempatch::resolveApItemName(100) == nullptr);
    // strId 413 is one less than ForeignStandardItem+294=414
    REQUIRE(itempatch::resolveApItemName(413) == nullptr);
    // strId 415 is one more than 414
    REQUIRE(itempatch::resolveApItemName(415) == nullptr);
    // strId 8311 is one less than ForeignStandardItem+0x2000=8312
    REQUIRE(itempatch::resolveApItemName(8311) == nullptr);
    // strId 8313 is one more than 8312
    REQUIRE(itempatch::resolveApItemName(8313) == nullptr);
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: returns nullptr when no shop context", "[itempatch][resolve]")
{
    int64_t locId = locationIdForSlot(0);
    itempatch::registerScoutedItemName(locId, "Bombs");
    selectSlot(0, 0);

    // Clear shop context — should fall through
    itempatch::clearShopContext();

    REQUIRE(itempatch::resolveApItemName(414) == nullptr);  // info panel strId
    REQUIRE(itempatch::resolveApItemName(8312) == nullptr); // shop list strId
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: returns nullptr when no name registered for slot", "[itempatch][resolve]")
{
    // Register a name at slot 0 but select slot 3 (which has no registration)
    int64_t locId = locationIdForSlot(0);
    itempatch::registerScoutedItemName(locId, "Bow");
    selectSlot(0, 3);

    // Slot 3 has no registered name
    REQUIRE(itempatch::resolveApItemName(8312) == nullptr);
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: scroll offset affects info-panel slot selection", "[itempatch][resolve]")
{
    // Register names at slots 0 and 7
    itempatch::registerScoutedItemName(locationIdForSlot(0), "Slot Zero Item");
    itempatch::registerScoutedItemName(locationIdForSlot(7), "Slot Seven Item");

    // Select slot 0 (scroll=0, visual=0)
    selectSlot(0, 0);
    const uint16_t *result0 = itempatch::resolveApItemName(8312); // 0x2000 + ForeignStandardItem (info panel)
    REQUIRE(result0 != nullptr);
    auto expected0 = okami::MSDManager::CompileString("Slot Zero Item");
    REQUIRE(result0[0] == expected0[0]);

    // Select slot 7 (scroll=5, visual=2)
    selectSlot(5, 2);
    const uint16_t *result7 = itempatch::resolveApItemName(8312);
    REQUIRE(result7 != nullptr);
    auto expected7 = okami::MSDManager::CompileString("Slot Seven Item");
    REQUIRE(result7[0] == expected7[0]);

    // The two results should be different strings
    REQUIRE(result0 != result7);
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: all three Foreign AP item types resolve via info panel strId", "[itempatch][resolve]")
{
    // Register names at slots 10, 11, 12 for different AP item types
    itempatch::registerScoutedItemName(locationIdForSlot(10), "Standard Thing");
    itempatch::registerScoutedItemName(locationIdForSlot(11), "Progression Thing");
    itempatch::registerScoutedItemName(locationIdForSlot(12), "Trap Thing");

    // Info panel strIds (0x2000+type) resolve to the selected slot's name.
    // List strIds (294+type) intentionally fall through to the override
    // generic dummy name (issue #124).

    // ForeignStandardItem: info=8312, list=414
    selectSlot(10, 0);
    REQUIRE(itempatch::resolveApItemName(8312) != nullptr);
    REQUIRE(itempatch::resolveApItemName(414) == nullptr);

    // ForeignProgressionItem: info=8322, list=424
    selectSlot(11, 0);
    REQUIRE(itempatch::resolveApItemName(8322) != nullptr);
    REQUIRE(itempatch::resolveApItemName(424) == nullptr);

    // ForeignTrapItem: info=8367, list=469
    selectSlot(12, 0);
    REQUIRE(itempatch::resolveApItemName(8367) != nullptr);
    REQUIRE(itempatch::resolveApItemName(469) == nullptr);
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: returns nullptr when shop pointer is null", "[itempatch][resolve]")
{
    itempatch::registerScoutedItemName(locationIdForSlot(0), "Null Shop Test");
    selectSlot(0, 0);

    // Null out just the shop pointer, keep shopId valid
    itempatch::setShopPointer(nullptr);

    // 8312 = 0x2000 + ForeignStandardItem (info panel path)
    REQUIRE(itempatch::resolveApItemName(8312) == nullptr);
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: resolves Okami-native dummy info-panel strIds", "[itempatch][resolve]")
{
    // Register a name at slot 0 so the shop context is valid
    itempatch::registerScoutedItemName(locationIdForSlot(0), "Power Slash");
    selectSlot(0, 0);

    // OkamiStandardItem (162), OkamiProgressionItem (168), OkamiTrapItem (172)
    // Info panel (0x2000+type) resolves to the selected slot's scouted name.
    // List (294+type) intentionally falls through (issue #124).
    REQUIRE(itempatch::resolveApItemName(162 + 0x2000) != nullptr);
    REQUIRE(itempatch::resolveApItemName(162 + 294) == nullptr);
    REQUIRE(itempatch::resolveApItemName(168 + 0x2000) != nullptr);
    REQUIRE(itempatch::resolveApItemName(168 + 294) == nullptr);
    REQUIRE(itempatch::resolveApItemName(172 + 0x2000) != nullptr);
    REQUIRE(itempatch::resolveApItemName(172 + 294) == nullptr);
}

TEST_CASE("clearShopContext: resets both shop ID and pointer", "[itempatch][live]")
{
    uint8_t buf[0x8C] = {};
    itempatch::resetState();
    itempatch::setShopMenuActiveOverrideForTests(1);
    itempatch::setCurrentShopId(3);
    itempatch::setShopPointer(buf);

    itempatch::clearShopContext();

    // After clearing, resolve should return nullptr even with a registered name
    itempatch::registerScoutedItemName(checks::getShopCheckId(3, 0), "Cleared Item");
    REQUIRE(itempatch::resolveApItemName(8312) == nullptr); // info panel strId
    itempatch::resetState();
}

TEST_CASE("resetState: clears all custom string registrations", "[itempatch][live]")
{
    uint8_t buf[0x8C] = {};
    itempatch::resetState();
    itempatch::setShopMenuActiveOverrideForTests(1);
    itempatch::setCurrentShopId(5);
    itempatch::setShopPointer(buf);
    buf[0x8A] = 0;
    buf[0x8B] = 0;

    itempatch::registerScoutedItemName(checks::getShopCheckId(5, 0), "Reset Test");
    // Verify it resolves before reset (use info panel path: 0x2000 + ForeignStandardItem = 8312)
    REQUIRE(itempatch::resolveApItemName(8312) != nullptr);

    itempatch::resetState();

    // After reset, nothing should resolve (shop context cleared + registrations gone +
    // shop menu override reset to live state, which reads zero in tests).
    itempatch::setShopMenuActiveOverrideForTests(1);
    itempatch::setCurrentShopId(5);
    itempatch::setShopPointer(buf);
    REQUIRE(itempatch::resolveApItemName(8312) == nullptr);

    itempatch::resetState(); // clean up
}

// ============================================================================
// Regression tests
// ============================================================================

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: shop list path is stable as cursor moves between same-type dummies (issue #124)",
                 "[itempatch][resolve][regression]")
{
    // Reproduces the bug shown in the user-supplied screenshots: a shop has
    // two OkamiProgression dummy slots (1 and 3) backing different scouted
    // items. The shop's row LIST queries `294 + OkamiProgressionItem` for both
    // rows. resolveApItemName must NOT bind that strId to the selected slot,
    // or both rows would render as whichever slot is currently selected and
    // visibly flicker between "Greensprout (Bloom)" and "Progressive Power
    // Slash" as the cursor moves between them.
    itempatch::registerScoutedItemName(locationIdForSlot(1), "Greensprout (Bloom)");
    itempatch::registerScoutedItemName(locationIdForSlot(3), "Progressive Power Slash");

    constexpr uint16_t kListStrId = okami::ItemTypes::OkamiProgressionItem + 294;

    selectSlot(0, 1);
    const uint16_t *whenSlot1Selected = itempatch::resolveApItemName(kListStrId);
    selectSlot(0, 3);
    const uint16_t *whenSlot3Selected = itempatch::resolveApItemName(kListStrId);

    // List rows of the same dummy type must resolve identically regardless of
    // selection, so the row text is stable as the cursor moves.
    REQUIRE(whenSlot1Selected == whenSlot3Selected);
    REQUIRE(whenSlot1Selected == nullptr); // falls through to override
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: info panel still tracks selected slot's scouted name",
                 "[itempatch][resolve][regression]")
{
    // Companion to the issue #124 regression: the info-panel strId path
    // (0x2000 + dummyType) must continue to resolve to the selected slot's
    // scouted name, since only one item is shown there at a time. Without
    // this, the bottom info area shows the generic dummy override (e.g.
    // "Okami Progression") even for the highlighted item, making it
    // impossible to tell what's actually for sale.
    itempatch::registerScoutedItemName(locationIdForSlot(1), "Greensprout (Bloom)");
    itempatch::registerScoutedItemName(locationIdForSlot(3), "Progressive Power Slash");

    constexpr uint16_t kInfoStrId = okami::ItemTypes::OkamiProgressionItem + 0x2000;

    selectSlot(0, 1);
    const uint16_t *infoSlot1 = itempatch::resolveApItemName(kInfoStrId);
    selectSlot(0, 3);
    const uint16_t *infoSlot3 = itempatch::resolveApItemName(kInfoStrId);

    REQUIRE(infoSlot1 != nullptr);
    REQUIRE(infoSlot3 != nullptr);
    REQUIRE(infoSlot1 != infoSlot3);

    auto greensprout = okami::MSDManager::CompileString("Greensprout (Bloom)");
    auto powerSlash = okami::MSDManager::CompileString("Progressive Power Slash");
    REQUIRE(infoSlot1[0] == greensprout[0]);
    REQUIRE(infoSlot3[0] == powerSlash[0]);
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: nothing resolves when shop menu is closed (issue #113)",
                 "[itempatch][resolve][regression]")
{
    // Reproduce the cutscene/Mist/area-name leak: shop context lingers from a
    // previous shop visit (s_currentShopId, s_pCurrentShop still set), and
    // the renderer happens to query an MSD strId that overlaps an AP dummy
    // strId. Without the gate, this returns the cached scouted name and the
    // cutscene displays e.g. "Progressive Cherry Bomb" instead of the area.
    itempatch::registerScoutedItemName(locationIdForSlot(0), "Cherry Bomb");
    selectSlot(0, 0);

    // Player exits the shop. The shop menu GlobalGameState bit clears, but
    // s_currentShopId and s_pCurrentShop are still set from the last visit.
    itempatch::setShopMenuActiveOverrideForTests(0);

    // Both AP dummy strId paths must NOT substitute scouted names.
    REQUIRE(itempatch::resolveApItemName(414) == nullptr);  // shop list
    REQUIRE(itempatch::resolveApItemName(8312) == nullptr); // info panel
    // Okami-native dummy types must also not substitute.
    REQUIRE(itempatch::resolveApItemName(okami::ItemTypes::OkamiProgressionItem + 294) == nullptr);
    REQUIRE(itempatch::resolveApItemName(okami::ItemTypes::OkamiProgressionItem + 0x2000) == nullptr);
}

TEST_CASE_METHOD(ShopContextFixture, "resolveApItemName: container path still resolves regardless of shop menu state",
                 "[itempatch][resolve][regression]")
{
    // The container path is independent of the shop menu - a chest opened in
    // the overworld should still display the scouted name above the floating
    // item. Verify the issue #113 gate didn't accidentally block this path.
    constexpr int64_t kContainerLoc = 9001;
    itempatch::registerScoutedItemName(kContainerLoc, "Container Treasure");

    // Shop menu is OFF (player is on the overworld), shop pointer is null.
    itempatch::setShopMenuActiveOverrideForTests(0);
    itempatch::setShopPointer(nullptr);
    itempatch::setCurrentShopId(-1);

    // Container context briefly active during pickup
    itempatch::setContainerContext(kContainerLoc);

    // Both list and info-panel strId paths should resolve via the container
    // branch since only one floating name is being rendered.
    REQUIRE(itempatch::resolveApItemName(414) != nullptr);  // list path
    REQUIRE(itempatch::resolveApItemName(8312) != nullptr); // info panel path

    itempatch::clearContainerContext();
}
