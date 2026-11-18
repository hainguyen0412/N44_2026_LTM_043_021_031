#include "mavlink_gateway/mission.hpp"
#include <common/mavlink.h>
#include <cmath>
#include <cstring>

namespace mavlink_gateway {
namespace {
mavlink_message_t message(const FrameRecord& f) {
    mavlink_message_t m{}; m.msgid=f.header.msgid; m.len=f.header.payload_len;
    if(f.raw.size()>=std::size_t(10+m.len)) std::memcpy(_MAV_PAYLOAD_NON_CONST(&m),f.raw.data()+10,m.len);
    return m;
}
std::uint8_t normalized_frame(std::uint8_t f) { return f==3?6:f==0?5:f; }
}
std::optional<MissionPacket> decode_mission(const FrameRecord& f) {
    const auto m=message(f); MissionPacket p; p.msgid=m.msgid;
    switch(m.msgid) {
    case 43: {mavlink_mission_request_list_t v{};mavlink_msg_mission_request_list_decode(&m,&v);p.target_system=v.target_system;p.target_component=v.target_component;p.mission_type=v.mission_type;break;}
    case 40: {mavlink_mission_request_t v{};mavlink_msg_mission_request_decode(&m,&v);p.seq=v.seq;p.target_system=v.target_system;p.target_component=v.target_component;p.mission_type=v.mission_type;break;}
    case 51: {mavlink_mission_request_int_t v{};mavlink_msg_mission_request_int_decode(&m,&v);p.seq=v.seq;p.target_system=v.target_system;p.target_component=v.target_component;p.mission_type=v.mission_type;break;}
    case 44: {mavlink_mission_count_t v{};mavlink_msg_mission_count_decode(&m,&v);p.count=v.count;p.target_system=v.target_system;p.target_component=v.target_component;p.mission_type=v.mission_type;p.opaque_id=v.opaque_id;break;}
    case 47: {mavlink_mission_ack_t v{};mavlink_msg_mission_ack_decode(&m,&v);p.result=v.type;p.target_system=v.target_system;p.target_component=v.target_component;p.mission_type=v.mission_type;p.opaque_id=v.opaque_id;break;}
    case 73: {mavlink_mission_item_int_t v{};mavlink_msg_mission_item_int_decode(&m,&v);p.target_system=v.target_system;p.target_component=v.target_component;p.mission_type=v.mission_type;p.seq=v.seq;p.item={v.seq,v.command,v.frame,v.current,v.autocontinue,{v.param1,v.param2,v.param3,v.param4},v.x,v.y,v.z};break;}
    case 39: {mavlink_mission_item_t v{};mavlink_msg_mission_item_decode(&m,&v);if(!std::isfinite(v.x)||!std::isfinite(v.y)||std::abs(v.x)>90||std::abs(v.y)>180)return std::nullopt;p.target_system=v.target_system;p.target_component=v.target_component;p.mission_type=v.mission_type;p.seq=v.seq;p.item={v.seq,v.command,v.frame,v.current,v.autocontinue,{v.param1,v.param2,v.param3,v.param4},static_cast<std::int32_t>(std::llround(v.x*1e7)),static_cast<std::int32_t>(std::llround(v.y*1e7)),v.z};break;}
    case 42: {mavlink_mission_current_t v{};mavlink_msg_mission_current_decode(&m,&v);p.seq=v.seq;p.count=v.total;p.opaque_id=v.mission_id;break;}
    case 46: {mavlink_mission_item_reached_t v{};mavlink_msg_mission_item_reached_decode(&m,&v);p.seq=v.seq;break;}
    default:return std::nullopt;
    }
    return p;
}
std::optional<HomePosition> decode_home(const FrameRecord& f) {
    if(f.header.msgid!=242)return std::nullopt;
    const auto m=message(f);mavlink_home_position_t v{};mavlink_msg_home_position_decode(&m,&v);
    return HomePosition{v.latitude/1e7,v.longitude/1e7,v.altitude/1000.0};
}
std::optional<std::uint8_t> decode_landed_state(const FrameRecord& f) {
    if(f.header.msgid!=245)return std::nullopt;
    const auto m=message(f);mavlink_extended_sys_state_t v{};mavlink_msg_extended_sys_state_decode(&m,&v);return v.landed_state;
}
bool same_mission_item(const MissionItem& a,const MissionItem& b) {
    if(a.seq!=b.seq||a.command!=b.command||normalized_frame(a.frame)!=normalized_frame(b.frame)||a.autocontinue!=b.autocontinue||
       std::llabs(static_cast<long long>(a.latitude_e7)-b.latitude_e7)>1||std::llabs(static_cast<long long>(a.longitude_e7)-b.longitude_e7)>1||
       !std::isfinite(b.altitude)||std::abs(a.altitude-b.altitude)>.02f)return false;
    for(std::size_t i=0;i<4;++i) {
        if(std::isnan(a.params[i])&&std::isnan(b.params[i]))continue;
        if(!std::isfinite(a.params[i])||!std::isfinite(b.params[i])||std::abs(a.params[i]-b.params[i])>.001f)return false;
    }
    return true;
}
} // namespace mavlink_gateway
