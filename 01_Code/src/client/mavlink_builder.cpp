#include "mavlink_builder.hpp"
#include <common/mavlink.h>
#include <mutex>

namespace cclient {
using mavlink_gateway::Bytes;
namespace {
// Thu vien MAVLink C giu bo dem SEQ tinh; khoa de nhieu thread khong dua nhau.
std::mutex g_mutex;

mavlink_gateway::Bytes to_bytes(const mavlink_message_t& msg) {
    std::uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const std::uint16_t n = mavlink_msg_to_send_buffer(buf, &msg);
    return mavlink_gateway::Bytes(buf, buf + n);
}
} // namespace

mavlink_gateway::Bytes build_gcs_heartbeat(const GcsId& gcs) {
    std::lock_guard<std::mutex> lock(g_mutex);
    mavlink_message_t msg{};
    mavlink_msg_heartbeat_pack(gcs.sysid, gcs.compid, &msg, MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID,
                               0, 0, MAV_STATE_ACTIVE);
    return to_bytes(msg);
}

mavlink_gateway::Bytes build_command_long(const GcsId& gcs, std::uint8_t target_system,
                                          std::uint8_t target_component, std::uint16_t command,
                                          const std::array<float, 7>& p,
                                          std::uint8_t confirmation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    mavlink_message_t msg{};
    mavlink_msg_command_long_pack(gcs.sysid, gcs.compid, &msg, target_system, target_component,
                                  command, confirmation, p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
    return to_bytes(msg);
}
Bytes build_mission_count(const GcsId& g,std::uint8_t s,std::uint8_t c,std::uint16_t n) {
    std::lock_guard<std::mutex> lock(g_mutex);mavlink_message_t m{};
    mavlink_msg_mission_count_pack(g.sysid,g.compid,&m,s,c,n,0,0);return to_bytes(m);
}
Bytes build_mission_request_list(const GcsId& g,std::uint8_t s,std::uint8_t c) {
    std::lock_guard<std::mutex> lock(g_mutex);mavlink_message_t m{};
    mavlink_msg_mission_request_list_pack(g.sysid,g.compid,&m,s,c,0);return to_bytes(m);
}
Bytes build_mission_request(const GcsId& g,std::uint8_t s,std::uint8_t c,std::uint16_t seq) {
    std::lock_guard<std::mutex> lock(g_mutex);mavlink_message_t m{};
    mavlink_msg_mission_request_int_pack(g.sysid,g.compid,&m,s,c,seq,0);return to_bytes(m);
}
Bytes build_mission_item(const GcsId& g,std::uint8_t s,std::uint8_t c,const mavlink_gateway::MissionItem& v) {
    std::lock_guard<std::mutex> lock(g_mutex);mavlink_message_t m{};
    mavlink_msg_mission_item_int_pack(g.sysid,g.compid,&m,s,c,v.seq,v.frame,v.command,v.current,v.autocontinue,v.params[0],v.params[1],v.params[2],v.params[3],v.latitude_e7,v.longitude_e7,v.altitude,0);return to_bytes(m);
}
Bytes build_mission_ack(const GcsId& g,std::uint8_t s,std::uint8_t c,std::uint8_t result) {
    std::lock_guard<std::mutex> lock(g_mutex);mavlink_message_t m{};
    mavlink_msg_mission_ack_pack(g.sysid,g.compid,&m,s,c,result,0,0);return to_bytes(m);
}
} // namespace cclient
