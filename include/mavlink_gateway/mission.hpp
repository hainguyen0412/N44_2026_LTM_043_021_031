#pragma once
#include "contracts.hpp"
#include <array>
#include <limits>

namespace mavlink_gateway {
// Separate extension: contracts.hpp and its five-message variant remain unchanged.
struct MissionItem {
    std::uint16_t seq = 0, command = 16;
    std::uint8_t frame = 6, current = 0, autocontinue = 1;
    std::array<float, 4> params{0, 0, 0, std::numeric_limits<float>::quiet_NaN()};
    std::int32_t latitude_e7 = 0, longitude_e7 = 0;
    float altitude = 0;
};
struct MissionPacket {
    std::uint32_t msgid = 0;
    std::uint8_t target_system = 0, target_component = 0, mission_type = 0, result = 0;
    std::uint16_t seq = 0, count = 0;
    std::uint32_t opaque_id = 0;
    MissionItem item;
};
struct HomePosition { double latitude = 0, longitude = 0, altitude_msl = 0; };
std::optional<MissionPacket> decode_mission(const FrameRecord& frame);
std::optional<HomePosition> decode_home(const FrameRecord& frame);
std::optional<std::uint8_t> decode_landed_state(const FrameRecord& frame);
bool same_mission_item(const MissionItem& a, const MissionItem& b);
} // namespace mavlink_gateway
