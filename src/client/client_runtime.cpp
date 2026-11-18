#include "client_runtime.hpp"
#include "client_log.hpp"
#include "client_metrics.hpp"
#include "send_queue.hpp"
#include "home_reference.hpp"
#include "mavlink_gateway/processing.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace cclient {
using namespace mavlink_gateway;
namespace {
constexpr double rad_to_deg=57.29577951308232;
std::string mode_name(std::uint32_t custom){
    const auto main=(custom>>16)&255,sub=(custom>>24)&255;
    if(main==4){switch(sub){case 2:return "AUTO TAKEOFF";case 3:return "HOLD";case 4:return "MISSION";case 5:return "RETURN";case 6:return "LAND";default:return "AUTO";}}
    switch(main){case 1:return "MANUAL";case 2:return "ALTITUDE";case 3:return "POSITION";case 6:return "OFFBOARD";default:return "PX4 mode "+std::to_string(main);}
}
bool finite_position(const GlobalPosition& p){return std::isfinite(p.latitude_deg)&&std::isfinite(p.longitude_deg)&&std::abs(p.latitude_deg)<=90&&std::abs(p.longitude_deg)<=180;}
}
ClientRuntime::MonoNs ClientRuntime::now()const{return static_cast<MonoNs>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-origin_).count());}
ClientRuntime::ClientRuntime(ClientOptions options):base_(std::move(options)){
    state_={{"connected",false},{"role","observer"},{"telemetry",Json::object()},{"command",{{"busy",false}}},{"mission",{{"busy",false},{"state","idle"}}},{"mission_enabled",base_.shared.mission_enabled}};
}
ClientRuntime::~ClientRuntime(){disconnect();}
void ClientRuntime::event(const std::string& message){events_.push_back({{"time",std::to_string(now()/1000000000ULL)+"s"},{"message",message}});if(events_.size()>64)events_.erase(events_.begin());}
void ClientRuntime::stop_locked(){
    running_=false;if(socket_.valid())socket_.shutdown_both();
    if(worker_.joinable())worker_.join();
    socket_.close();
    std::lock_guard<std::mutex> lock(mutex_);state_["connected"]=false;heartbeat_at_=0;requests_.clear();
}
void ClientRuntime::disconnect(){std::lock_guard<std::mutex> life(lifecycle_);stop_locked();}
bool ClientRuntime::connect(bool controller,std::string& error){
    std::lock_guard<std::mutex> life(lifecycle_);stop_locked();
    auto options=base_;options.controller=controller;options.port=controller?options.shared.controller_port:options.shared.observer_port;
    auto sock=cnet::Socket::tcp_connect(options.host,options.port,&error);
    if(!sock.valid()){std::lock_guard<std::mutex> lock(mutex_);event("Khong noi gateway: "+error);return false;}
    socket_=std::move(sock);controller_=controller;++connection_id_;
    if(options.run_id.empty())options.run_id="web_"+std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    options.name="web_"+std::string(controller?"controller_":"observer_")+std::to_string(connection_id_);
    Json resolved=Json::parse(options.resolved_config_json);resolved["effective_client"]["logging"]["run_id"]=options.run_id;resolved["effective_client"]["logging"]["component"]=options.name;resolved["web_runtime"]={{"role",controller?"controller":"observer"},{"host",options.host},{"port",options.port},{"component",options.name}};options.resolved_config_json=resolved.dump();
    {std::lock_guard<std::mutex> lock(mutex_);heartbeat_at_=0;requests_.clear();state_={{"connected",true},{"role",controller?"controller":"observer"},{"session",connection_id_},{"telemetry",Json::object()},{"command",{{"busy",false}}},{"mission",{{"busy",false},{"state","idle"}}},{"mission_enabled",options.shared.mission_enabled},{"log_directory",options.shared.logging_enabled?(std::filesystem::path(options.shared.output_root)/options.run_id/options.name).string():""},{"logging_enabled",options.shared.logging_enabled}};event("Da noi gateway "+options.host+":"+std::to_string(options.port));}
    running_=true;worker_=std::thread([this,options]{loop(options);});return true;
}
ClientRuntime::Json ClientRuntime::snapshot()const{
    std::lock_guard<std::mutex> lock(mutex_);auto result=state_;const auto time=now();
    result["heartbeat_age_ms"]=heartbeat_at_?Json((time-heartbeat_at_)/1000000ULL):Json(nullptr);
    result["fresh"]=state_.value("connected",false)&&heartbeat_at_&&time-heartbeat_at_<static_cast<MonoNs>(base_.shared.heartbeat_stale_ms)*1000000ULL;
    for(auto& item:result["telemetry"].items())if(item.value().is_object()&&item.value().contains("at_ns")){
        const auto at=item.value()["at_ns"].get<MonoNs>();
        item.value()["age_ms"]=(time-at)/1000000ULL;
        item.value()["fresh"]=state_.value("connected",false)&&time-at<(item.key()=="battery"?5000000000ULL:3000000000ULL);
    }
    result["events"]=events_;result["server_time_ns"]=time;return result;
}
void ClientRuntime::check_control()const{
    if(!state_.value("connected",false)||!running_)throw std::runtime_error("Chua ket noi gateway.");
    if(!controller_)throw std::runtime_error("Observer khong duoc gui lenh.");
    if(!heartbeat_at_||now()-heartbeat_at_>=static_cast<MonoNs>(base_.shared.heartbeat_stale_ms)*1000000ULL)throw std::runtime_error("Chua co heartbeat PX4 tuoi.");
    if(!requests_.empty()||state_["command"].value("busy",false)||state_["mission"].value("busy",false))throw std::runtime_error("Dang xu ly lenh/mission; vui long cho.");
}
void ClientRuntime::enqueue(Request r){state_.erase("request_error");requests_.push_back(std::move(r));}
void ClientRuntime::command(const std::string& name,const std::string& id){
    std::lock_guard<std::mutex> lock(mutex_);check_control();
    if(name!="ARM"&&name!="DISARM"&&name!="TAKEOFF"&&name!="LAND")throw std::invalid_argument("Lenh khong duoc ho tro.");
    const auto command=name=="ARM"||name=="DISARM"?400:name=="TAKEOFF"?22:21;
    if(std::find(base_.shared.allowed_commands.begin(),base_.shared.allowed_commands.end(),command)==base_.shared.allowed_commands.end())throw std::runtime_error("Lenh bi tat boi cau hinh.");
    if(name=="TAKEOFF"&&!state_["telemetry"].value("heartbeat",Json::object()).value("armed",false))throw std::runtime_error("Can ARM truoc khi TAKEOFF.");
    enqueue({name,id,{},{}});
}
void ClientRuntime::upload(std::vector<MissionItem> items,Json draft,const std::string& id){
    std::lock_guard<std::mutex> lock(mutex_);check_control();
    if(!base_.shared.mission_enabled)throw std::runtime_error("Mission chua bat trong cau hinh gateway/client.");
    if(state_["telemetry"].value("heartbeat",Json::object()).value("armed",false))throw std::runtime_error("Chi tai mission khi drone DISARMED.");
    enqueue({"upload",id,std::move(items),std::move(draft)});
}
void ClientRuntime::readback(const std::string& id){
    std::lock_guard<std::mutex> lock(mutex_);check_control();if(!base_.shared.mission_enabled)throw std::runtime_error("Mission chua bat.");
    if(state_["telemetry"].value("heartbeat",Json::object()).value("armed",false))throw std::runtime_error("Doc lai thu cong khi DISARMED.");
    enqueue({"readback",id,{},{}});
}
void ClientRuntime::start_mission(const std::string& id,const std::string& upload_id){
    std::lock_guard<std::mutex> lock(mutex_);check_control();
    if(!base_.shared.mission_enabled||std::find(base_.shared.allowed_commands.begin(),base_.shared.allowed_commands.end(),300)==base_.shared.allowed_commands.end())throw std::runtime_error("MISSION_START chua duoc cho phep.");
    const auto& telemetry=state_["telemetry"];
    if(!telemetry.value("heartbeat",Json::object()).value("armed",false))throw std::runtime_error("Can ARM truoc khi Start.");
    if(telemetry.value("landed_state",0)!=1)throw std::runtime_error("Can EXTENDED_SYS_STATE xac nhan drone dang tren mat dat.");
    if(now()-telemetry.value("landed_at_ns",MonoNs{0})>=3000000000ULL)throw std::runtime_error("EXTENDED_SYS_STATE da cu; can du lieu moi truoc Start.");
    if(!state_["mission"].value("verified",false)||state_["mission"].value("consumed",false))throw std::runtime_error("Can tai va xac minh mission moi truoc khi Start.");
    if(upload_id.empty()||state_["mission"].value("upload_request_id","")!=upload_id)throw std::runtime_error("Start khong khop luot upload da xac minh.");
    enqueue({"start",id,{},{}});
}
void ClientRuntime::loop(ClientOptions options){
    try{
        ClientMetrics metrics(std::uint64_t(options.shared.warmup_seconds)*1000000000ULL,std::uint64_t(options.shared.measurement_seconds)*1000000000ULL,std::uint64_t(options.shared.drain_ms)*1000000ULL,100000,[this]{return now();});
        ClientLog log(options,[this]{return now();});ProcessProbe process;metrics.connected();
        Parser parser(options.shared.tcp_buffer_bytes);Decoder decoder;Inspector inspector;
        std::unique_ptr<SendQueue> queue;
        auto completed=[&](const SendCompletion& result){metrics.completed(result);log.tx_completed(result);};
        auto send=[&](const Bytes& bytes){const auto tag=metrics.attempt(now());log.tx_attempt(tag,bytes.size());if(queue&&queue->push(bytes,false,tag))return true;completed({tag,now(),bytes.size(),DeliveryStatus::ClientQueueFull});return false;};
        if(options.controller)queue=std::make_unique<SendQueue>(socket_,options.shared.client_queue_frames,options.shared.client_queue_bytes,0,0,[this]{return now();},completed);
        const GcsId gcs{options.command.gcs_sysid,options.command.gcs_compid};
        std::string current_request,upload_request;bool pending_start=false,was_fresh=false,seen_mission_mode=false;
        std::optional<HomePosition> mission_home;
        auto mission_changed=[&](const MissionStatus& s){state_["mission"]={{"state",s.state},{"message",s.message},{"busy",s.busy},{"verified",s.verified},{"consumed",s.consumed},{"operation_id",s.operation_id},{"total",s.total},{"next",s.next},{"current",s.current},{"reached",s.reached},{"request_id",current_request},{"upload_request_id",upload_request}};log.event("mission",state_["mission"].dump());event(s.message);};
        MissionConfig mission_cfg{gcs,options.command.px4_sysid,options.command.px4_compid,options.shared.mission_max_items,static_cast<MonoNs>(options.shared.mission_timeout_ms)*1000000ULL,options.shared.mission_max_attempts};
        MissionManager mission(mission_cfg,send,mission_changed);
        CommandManager manager(options.command,[&](std::uint16_t cmd,const Params& p,std::uint8_t conf){return build_command_long(gcs,options.command.px4_sysid,options.command.px4_compid,cmd,p,conf);},send,[&](const CommandEvent& e){
            log.command(e);if(e.event=="ack_ignored"||e.event=="ack_late")return;
            const bool busy=e.event=="submitted"||e.event=="queued"||e.event=="ack_in_progress";
            state_["command"]={{"command",e.command},{"request_id",e.request_id},{"http_request_id",current_request},{"event",e.event},{"attempt",e.attempt},{"busy",busy},{"result",e.result?Json(*e.result):Json(nullptr)}};event("Command "+std::to_string(e.command)+": "+e.event);
        });
        MonoNs heartbeat_due=0,metrics_due=0;FrameId frame_id=1;std::uint32_t boot_ms=0;std::uint8_t buffer[8192];
        while(running_){
            const auto time=now();
            {std::lock_guard<std::mutex> lock(mutex_);
                if(!requests_.empty()){
                    auto request=std::move(requests_.front());requests_.pop_front();current_request=request.id;
                    try{
                        if(!heartbeat_at_||time-heartbeat_at_>=static_cast<MonoNs>(options.shared.heartbeat_stale_ms)*1000000ULL)throw std::runtime_error("PX4 heartbeat da het han.");
                        const bool armed=state_["telemetry"].value("heartbeat",Json::object()).value("armed",false);
                        if(request.type=="upload"){
                            if(armed)throw std::runtime_error("PX4 da ARMED; huy yeu cau upload.");
                            const auto current_home=state_["telemetry"].value("home",Json::object());
                            const auto expected_home=request.draft.at("_home");
                            if(!current_home.contains("latitude"))throw std::runtime_error("Chua co Home PX4.");
                            const HomePosition reference{expected_home.at("latitude"),expected_home.at("longitude"),expected_home.at("altitude_msl")};
                            const HomePosition current{current_home.at("latitude"),current_home.at("longitude"),current_home.at("altitude_msl")};
                            const auto delta=compare_home(reference,current);
                            if(delta.changed())throw std::runtime_error("Home da thay doi sau khi bien dich mission ("+delta.description()+").");
                            mission_home=reference;
                            state_["mission_home"]=expected_home;
                            upload_request=request.id;seen_mission_mode=false;
                            log.event("mission_draft",Json{{"request_id",request.id},{"draft",request.draft}}.dump());
                            for(const auto& item:request.items)
                                log.event("mission_item",Json{{"request_id",request.id},{"seq",item.seq},{"command",item.command},{"frame",item.frame},{"current",item.current},{"autocontinue",item.autocontinue},{"params",item.params},{"latitude_e7",item.latitude_e7},{"longitude_e7",item.longitude_e7},{"altitude_relative_m",item.altitude}}.dump());
                            if(!mission.upload(std::move(request.items),time))throw std::runtime_error("Mission khong hop le hoac khong gui duoc.");
                        }else if(request.type=="readback")mission.readback(time);
                        else if(request.type=="start"){
                            if(!armed||state_["telemetry"].value("landed_state",0)!=1||!mission.status().verified||mission.status().consumed)throw std::runtime_error("Dieu kien Start da thay doi.");
                            pending_start=true;mission.readback(time);
                        }else{
                            Params p{};std::uint16_t cmd=0;
                            if(request.type=="ARM"||request.type=="DISARM"){cmd=kCmdArm;p[0]=request.type=="ARM"?1.f:0.f;}
                            else {cmd=request.type=="TAKEOFF"?kCmdTakeoff:kCmdLand;p[3]=p[4]=p[5]=p[6]=std::numeric_limits<float>::quiet_NaN();}
                            if(cmd==kCmdTakeoff&&!armed)throw std::runtime_error("PX4 chua ARMED.");
                            if(request.type=="LAND"||request.type=="DISARM")pending_start=false;
                            manager.submit(cmd,p,time);
                        }
                    }catch(const std::exception& e){pending_start=false;event(e.what());log.event("request_failed",e.what());state_["request_error"]=e.what();}
                }
                const bool fresh=heartbeat_at_&&time-heartbeat_at_<static_cast<MonoNs>(options.shared.heartbeat_stale_ms)*1000000ULL;
                if(was_fresh&&!fresh){pending_start=false;mission.invalidate("PX4 heartbeat stale; can xac minh lai.");if(mission.status().busy||manager.busy()){running_=false;break;}}
                was_fresh=fresh;manager.tick(time);mission.tick(time);
                if(pending_start&&!mission.status().busy){
                    pending_start=false;
                    if(mission.status().verified&&fresh&&state_["telemetry"].value("heartbeat",Json::object()).value("armed",false)&&state_["telemetry"].value("landed_state",0)==1&&time-state_["telemetry"].value("landed_at_ns",MonoNs{0})<3000000000ULL){
                        Params p{};p[0]=0;if(mission.consume_verified())manager.submit(kCmdMissionStart,p,time);
                    }else {mission.invalidate("Dieu kien Start/readback khong con hop le.");}
                }
                if(mission.status().state=="unknown"||mission.status().state=="failed"){running_=false;break;}
                if(options.controller&&time>=heartbeat_due){send(build_gcs_heartbeat(gcs));heartbeat_due=time+static_cast<MonoNs>(options.heartbeat_interval_ms)*1000000ULL;}
                if(time>=metrics_due){log.metrics(time,metrics.snapshot(),inspector.snapshot(),queue?queue->snapshot():SendQueueSnapshot{},process.sample(),true);metrics_due=time+static_cast<MonoNs>(options.shared.snapshot_interval_ms)*1000000ULL;}
            }
            if(queue&&queue->failed())break;
            if(!socket_.readable(30))continue;
            const auto n=socket_.recv_some(buffer,sizeof(buffer));if(n<=0)break;
            ReceiveEvent ev;ev.source={1,connection_id_,Transport::Tcp,Flow::TelemetryDownlink,{options.host,options.port}};ev.received_at_ns=now();ev.bytes.assign(buffer,buffer+n);
            auto batch=parser.consume(ev);
            std::lock_guard<std::mutex> lock(mutex_);
            for(const auto& issue:batch.issues){if(options.mode!=MeasurementMode::B0)inspector.on_issue(issue);log.issue(issue);}
            for(auto& f:batch.frames){
                f.frame_id=frame_id++;metrics.received(f.received_at_ns);log.frame(f);
                const auto decoded=(options.mode!=MeasurementMode::B0||f.header.msgid==0||f.header.msgid==77)?decoder.decode(f):std::nullopt;
                if(options.mode!=MeasurementMode::B0)inspector.on_frame(f,decoded);
                if(f.header.sysid!=options.command.px4_sysid||f.header.compid!=options.command.px4_compid)continue;
                auto& t=state_["telemetry"];
                if(decoded){
                    if(const auto* h=std::get_if<Heartbeat>(&*decoded)){
                        heartbeat_at_=f.received_at_ns;const auto mode=mode_name(h->custom_mode);
                        t["heartbeat"]={{"armed",bool(h->base_mode&128)},{"mode",mode},{"custom_mode",h->custom_mode},{"status",h->system_status},{"at_ns",f.received_at_ns}};
                        if((h->base_mode&128)&&mission.status().busy&&!pending_start)
                            mission.invalidate("PX4 da ARMED trong luc upload/doc lai; dung giao dich va doi chieu lai.");
                        if(mode=="MISSION"){seen_mission_mode=true;if(mission.status().consumed){state_["mission"]["state"]="running";state_["mission"]["message"]="PX4 dang thuc hien mission; theo doi current/reached.";}}
                        if(mission.status().consumed&&seen_mission_mode&&!(h->base_mode&128)&&t.value("landed_state",0)==1&&f.received_at_ns-t.value("landed_at_ns",MonoNs{0})<3000000000ULL){state_["mission"]["state"]="completed";state_["mission"]["message"]="PX4 xac nhan da ha canh va DISARMED.";}
                    }else if(const auto* b=std::get_if<SysStatus>(&*decoded))t["battery"]={{"voltage",b->battery_voltage_v?Json(*b->battery_voltage_v):Json(nullptr)},{"percent",b->battery_remaining_percent?Json(*b->battery_remaining_percent):Json(nullptr)},{"at_ns",f.received_at_ns}};
                    else if(const auto* a=std::get_if<Attitude>(&*decoded))t["attitude"]={{"roll",a->roll_rad*rad_to_deg},{"pitch",a->pitch_rad*rad_to_deg},{"yaw",a->yaw_rad*rad_to_deg},{"at_ns",f.received_at_ns}};
                    else if(const auto* p=std::get_if<GlobalPosition>(&*decoded)){if(finite_position(*p))t["position"]={{"latitude",p->latitude_deg},{"longitude",p->longitude_deg},{"altitude_msl",p->altitude_msl_m},{"relative_altitude",p->relative_altitude_m},{"at_ns",f.received_at_ns}};}
                    else if(const auto* ack=std::get_if<CommandAck>(&*decoded))manager.on_ack(*ack,f.header.sysid,f.header.compid,f.received_at_ns);
                }
                if(f.header.msgid==30&&f.raw.size()>=14){std::uint32_t v=0;for(int i=0;i<4;++i)v|=std::uint32_t(f.raw[10+i])<<(8*i);if(boot_ms>v&&boot_ms-v>1000&&boot_ms-v<0x80000000U){t.erase("home");pending_start=false;mission.invalidate("PX4 time_boot_ms reset; can Home va mission moi.");running_=false;break;}boot_ms=v;}
                if(const auto home=decode_home(f)){
                    if(valid_home(*home)){
                        if(mission_home){
                            const auto delta=compare_home(*mission_home,*home);
                            if(delta.changed()){
                                pending_start=false;
                                mission.invalidate("Home PX4 da thay doi ("+delta.description()+"); can tai/xac minh lai.");
                                mission_home.reset();state_.erase("mission_home");
                            }
                        }
                        t["home"]={{"latitude",home->latitude},{"longitude",home->longitude},{"altitude_msl",home->altitude_msl},{"at_ns",f.received_at_ns}};
                    }
                }
                if(const auto landed=decode_landed_state(f)){t["landed_state"]=*landed;t["landed_at_ns"]=f.received_at_ns;}
                if(const auto packet=decode_mission(f)){mission.on_packet(f.header,*packet,f.received_at_ns);state_["mission"]["current"]=mission.status().current;state_["mission"]["reached"]=mission.status().reached;}
            }
            if(batch.close_tcp_session)break;
        }
        running_=false;metrics.stopped();
        {std::lock_guard<std::mutex> lock(mutex_);manager.on_disconnect(now());mission.disconnect();state_["connected"]=false;heartbeat_at_=0;event("Da dong ket noi gateway.");}
        SendQueueSnapshot tx;if(queue){queue->close();tx=queue->snapshot();}socket_.shutdown_both();
        log.metrics(now(),metrics.snapshot(),inspector.snapshot(),tx,process.sample(),false);
        log.finish(metrics.snapshot(true),tx,process.logical_cpus(),socket_.peer_name(),tx.pending_frames==0);
    }catch(const std::exception& e){
        running_=false;socket_.shutdown_both();std::lock_guard<std::mutex> lock(mutex_);state_["connected"]=false;heartbeat_at_=0;state_["error"]=e.what();event(std::string("Client worker: ")+e.what());
    }
}
}
