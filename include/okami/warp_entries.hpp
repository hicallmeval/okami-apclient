#pragma once

#include <cstdint>
#include <optional>

// Auto-generated from the game's per-map JMP loading-zone tables (one
// canonical entry-point per destination map, picked across all source maps).
// The Okami HD Steam binary hasn't shipped a content patch since 2017, so
// these values are fixed; do not edit by hand.

namespace okami::warp
{

/// One valid entry-point for a destination map, lifted from the engine's own
/// per-map JMP table. Coordinates are in the game's world-space units; facing
/// is stored in degrees (multiply by pi/180 to match
/// WarpData::facingDirection).
struct EntryCoords
{
    std::uint16_t mapID;
    std::int16_t x;
    std::int16_t y;
    std::int16_t z;
    std::int16_t facingDeg;
};

/// Sorted by mapID. 84 entries covering every destination map referenced
/// by any source map's JMP table.
inline constexpr EntryCoords kEntryCoords[] = {
    {0x0100, -1, -5017, 5158, 0},       // src=0x0122 key=0x00
    {0x0101, 2900, 248, -5090, -155},   // src=0x0122 key=0x01
    {0x0102, -1, 184, -1211, 0},        // src=0x0122 key=0x02
    {0x0103, -58, -48, 309, 162},       // src=0x0F01 key=0x02
    {0x0104, 0, -4769, -2790, 0},       // src=0x0106 key=0x00
    {0x0105, 1353, 51, 22, -90},        // src=0x0F07 key=0x01
    {0x0106, -965, 3047, -7948, 180},   // src=0x0104 key=0x01
    {0x0107, 8170, -10000, 10100, 90},  // src=0x0108 key=0x00
    {0x0108, -5080, 711, 1830, 90},     // src=0x0107 key=0x00
    {0x0109, 1131, 49, -1527, -13},     // src=0x010B key=0x00
    {0x010A, 0, 1, -48, 180},           // src=0x0F03 key=0x02
    {0x010B, 0, 0, 20, 0},              // src=0x0109 key=0x05
    {0x010C, 0, 7, -58, -180},          // src=0x0F02 key=0x09
    {0x010D, -3650, 200, -1300, -90},   // src=0x0107 key=0x03
    {0x010E, -26, 0, 1413, 0},          // src=0x0110 key=0x00
    {0x0110, 0, -19, 1173, -170},       // src=0x010E key=0x00
    {0x0111, 0, 0, 0, 0},               // src=0x010E key=0x01
    {0x0112, 2403, -324, 2990, -155},   // src=0x0111 key=0x01
    {0x0113, -1043, 2501, -8707, 180},  // src=0x0F0A key=0x0A
    {0x0114, 0, 31, -170, 180},         // src=0x0F08 key=0x08
    {0x0115, 0, 31, -170, -180},        // src=0x0F12 key=0x05
    {0x0116, 0, 31, -170, -180},        // src=0x0F0C key=0x05
    {0x0117, 0, 31, -170, -180},        // src=0x0F0C key=0x06
    {0x0118, 0, 31, -170, -180},        // src=0x0F13 key=0x05
    {0x0119, 0, 27, -28, 180},          // src=0x0F12 key=0x07
    {0x011A, -1043, 2501, -8707, 180},  // src=0x0F0C key=0x0B
    {0x011B, -1043, 2501, -8707, 180},  // src=0x0F12 key=0x08
    {0x011C, 0, 31, -170, -180},        // src=0x0203 key=0x13
    {0x011D, 0, 31, -170, -180},        // src=0x0F0C key=0x0D
    {0x0122, -13, -26, 238, -178},      // src=0x0100 key=0x00
    {0x0200, 0, 671, 1850, -180},       // src=0x0201 key=0x01
    {0x0201, -1009, -255, -830, 90},    // src=0x010B key=0x03
    {0x0202, 0, 0, 0, 0},               // src=0x0200 key=0x01
    {0x0203, 1166, 77, -1506, -110},    // src=0x010B key=0x02
    {0x0204, 0, -50, 0, 180},           // src=0x0203 key=0x11
    {0x0205, -1060, -34, 390, 120},     // src=0x0F0A key=0x04
    {0x0206, 0, 16, 231, 180},          // src=0x0200 key=0x02
    {0x0207, 735, -13, -1721, 180},     // src=0x0206 key=0x04
    {0x0208, 0, 0, -100, 180},          // src=0x020D key=0x01
    {0x0209, 0, -437, 1285, 0},         // src=0x020E key=0x01
    {0x020A, -88, 4, 668, 167},         // src=0x0F0C key=0x02
    {0x020B, 0, 0, 0, 0},               // src=0x0206 key=0x03
    {0x020C, 55, 0, 0, 0},              // src=0x0201 key=0x07
    {0x020D, 8, 300, 786, 0},           // src=0x0208 key=0x00
    {0x020E, 0, 691, 560, 180},         // src=0x0209 key=0x00
    {0x020F, -4811, -4996, -5422, -90}, // src=0x0208 key=0x04
    {0x0301, -120, 270, -1539, -21},    // src=0x0313 key=0x00
    {0x0302, 0, 208, -1540, 0},         // src=0x0311 key=0x03
    {0x0303, -3504, 4883, -1367, 180},  // src=0x0304 key=0x00
    {0x0304, 0, -61, -897, 0},          // src=0x0303 key=0x0D
    {0x0305, -805, 1413, 313, 77},      // src=0x030D key=0x00
    {0x0306, -2, -714, 3463, -180},     // src=0x0311 key=0x0D
    {0x0307, -591, -39, 3321, 169},     // src=0x0308 key=0x00
    {0x0308, -1008, 2501, -8700, -178}, // src=0x0307 key=0x03
    {0x0309, -7, 16, 503, 178},         // src=0x0307 key=0x04
    {0x030A, -179, 0, -60, 80},         // src=0x0307 key=0x05
    {0x030B, 0, 2, 424, 180},           // src=0x0307 key=0x01
    {0x030C, -4002, 186, -1315, -90},   // src=0x0307 key=0x02
    {0x030D, 0, 3, 35, 180},            // src=0x0305 key=0x07
    {0x0310, -862, 798, -15424, -168},  // src=0x0305 key=0x00
    {0x0311, -910, 797, -15411, 11},    // src=0x0302 key=0x01
    {0x0312, 0, 4, -300, 0},            // src=0x0307 key=0x06
    {0x0313, 6, 0, -427, 0},            // src=0x0301 key=0x02
    {0x0314, 0, 1, 0, 0},               // src=0x0303 key=0x0E
    {0x0E00, 0, 0, 0, 0},               // src=0x0200 key=0x13
    {0x0E01, 0, 0, 0, 0},               // src=0x0200 key=0x14
    {0x0E02, 0, 0, -10, 0},             // src=0x0F04 key=0x09
    {0x0E03, 0, 0, 0, 0},               // src=0x0F12 key=0x04
    {0x0E04, 0, 0, 0, 0},               // src=0x0F0C key=0x0C
    {0x0F01, -1521, -222, -765, 50},    // src=0x0102 key=0x00
    {0x0F02, -1521, -222, -765, 50},    // src=0x0102 key=0x01
    {0x0F03, -2386, 106, 3416, 89},     // src=0x010A key=0x00
    {0x0F04, 913, 0, 7414, -127},       // src=0x0104 key=0x00
    {0x0F06, 1800, 18077, 9850, 180},   // src=0x0110 key=0x02
    {0x0F07, -8280, 1042, 3220, 90},    // src=0x0105 key=0x02
    {0x0F08, -8280, 1042, 3220, 90},    // src=0x0105 key=0x03
    {0x0F09, 3383, 415, 1702, -77},     // src=0x0105 key=0x00
    {0x0F0A, 3383, 415, 1702, -77},     // src=0x0105 key=0x01
    {0x0F0C, 2251, -236, -12918, 178},  // src=0x0116 key=0x00
    {0x0F11, 1977, -9754, -2675, 0},    // src=0x0F02 key=0x08
    {0x0F12, 3004, -9998, 435, 180},    // src=0x010C key=0x02
    {0x0F13, -2714, 442, -3780, 0},     // src=0x0118 key=0x00
    {0x0F20, -1521, -222, -765, 50},    // src=0x0302 key=0x00
    {0x0F21, 1782, 18300, 5378, 0},     // src=0x0306 key=0x00
};

/// Look up the canonical entry coords for a destination map. Returns
/// std::nullopt if the map ID is not present in any JMP table.
[[nodiscard]] constexpr std::optional<EntryCoords> lookupEntry(std::uint16_t mapID) noexcept
{
    for (const auto &e : kEntryCoords)
    {
        if (e.mapID == mapID)
        {
            return e;
        }
    }
    return std::nullopt;
}

} // namespace okami::warp
