#pragma once
#include "mavlink_gateway/mission.hpp"
#include "mavlink_builder.hpp"
#include <functional>

namespace cclient {
struct MissionConfig {
    GcsId gcs;
    std::uint8_t target_system=1,target_component=1;
    std::size_t max_items=100;
    mavlink_gateway::MonoNs timeout_ns=1500000000ULL;
    unsigned max_attempts=5;
};
struct MissionStatus {
    std::string state="idle",message="Chua tai mission len PX4.";
    bool busy=false,verified=false,consumed=false;
    std::uint64_t operation_id=0;
    std::size_t total=0,next=0;
    int current=-1,reached=-1;
};
// Caller owns synchronization. All time is supplied for deterministic tests.
class MissionManager {
public:
    using Send=std::function<bool(const mavlink_gateway::Bytes&)>;
    using Notify=std::function<void(const MissionStatus&)>;
    MissionManager(MissionConfig,Send,Notify={});
    bool upload(std::vector<mavlink_gateway::MissionItem>,mavlink_gateway::MonoNs);
    bool readback(mavlink_gateway::MonoNs);
    void on_packet(const mavlink_gateway::FrameHeader&,const mavlink_gateway::MissionPacket&,mavlink_gateway::MonoNs);
    void tick(mavlink_gateway::MonoNs);
    void disconnect();
    void invalidate(const std::string&);
    bool consume_verified();
    const MissionStatus& status() const {return status_;}
    const std::vector<mavlink_gateway::MissionItem>& items() const {return expected_;}
    const std::vector<mavlink_gateway::MissionItem>& downloaded() const {return received_;}
private:
    enum class Phase {Idle,Upload,Count,Items};
    MissionConfig cfg_;Send send_;Notify notify_;MissionStatus status_;Phase phase_=Phase::Idle;
    std::vector<mavlink_gateway::MissionItem> expected_,received_;
    mavlink_gateway::Bytes last_;
    mavlink_gateway::MonoNs last_sent_=0,started_=0;
    unsigned attempts_=0;std::size_t offered_=0,download_count_=0;
    std::uint32_t plan_id_=0;
    void emit();
    bool transmit(mavlink_gateway::Bytes,mavlink_gateway::MonoNs,bool reset=true);
    void fail(const std::string&,bool unknown=false);
    void begin_read(mavlink_gateway::MonoNs);
};
}
