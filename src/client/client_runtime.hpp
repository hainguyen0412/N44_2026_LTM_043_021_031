#pragma once
#include "client_config.hpp"
#include "mission_manager.hpp"
#include "net_socket.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <deque>
#include <mutex>
#include <thread>

namespace cclient {
// Headless client adapter: shares Parser/Decoder/Inspector, transport, metrics,
// CommandManager and MissionManager with the C++ libraries; no stdin dependency.
class ClientRuntime {
public:
    using Json=nlohmann::json;
    explicit ClientRuntime(ClientOptions);
    ~ClientRuntime();
    bool connect(bool controller,std::string& error);
    void disconnect();
    Json snapshot() const;
    void command(const std::string& name,const std::string& request_id);
    void upload(std::vector<mavlink_gateway::MissionItem>,Json draft,const std::string& request_id);
    void readback(const std::string& request_id);
    void start_mission(const std::string& request_id,const std::string& upload_request_id);
private:
    using MonoNs=mavlink_gateway::MonoNs;
    struct Request {std::string type,id;std::vector<mavlink_gateway::MissionItem> items;Json draft;};
    ClientOptions base_;
    cnet::WinsockInit winsock_;
    cnet::Socket socket_;
    mutable std::mutex mutex_;
    std::mutex lifecycle_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    std::chrono::steady_clock::time_point origin_=std::chrono::steady_clock::now();
    Json state_,events_=Json::array();
    std::deque<Request> requests_;
    std::uint64_t connection_id_=0;
    MonoNs heartbeat_at_=0;
    bool controller_=false;
    MonoNs now() const;
    void stop_locked();
    void loop(ClientOptions);
    void event(const std::string&);
    void check_control() const;
    void enqueue(Request);
};
}
