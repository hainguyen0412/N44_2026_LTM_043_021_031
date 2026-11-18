#include "mission_manager.hpp"
#include <cmath>
#include <stdexcept>

namespace cclient {
using namespace mavlink_gateway;
MissionManager::MissionManager(MissionConfig cfg,Send send,Notify notify):cfg_(cfg),send_(std::move(send)),notify_(std::move(notify)) {}
void MissionManager::emit(){if(notify_)notify_(status_);}
void MissionManager::fail(const std::string& why,bool unknown){phase_=Phase::Idle;status_.busy=false;status_.verified=false;status_.state=unknown?"unknown":"failed";status_.message=why;last_.clear();emit();}
bool MissionManager::transmit(Bytes bytes,MonoNs now,bool reset){
    if(reset)attempts_=0;
    last_=std::move(bytes);last_sent_=now;++attempts_;
    if(!send_(last_)){fail("Khong xep hang duoc ban tin mission; can ket noi/xac minh lai.",true);return false;}return true;
}
bool MissionManager::upload(std::vector<MissionItem> items,MonoNs now){
    if(status_.busy)return false;
    if(items.size()<3||items.size()>cfg_.max_items)return false;
    for(std::size_t i=0;i<items.size();++i){
        auto& v=items[i];
        if(v.seq!=i||v.frame!=6||v.autocontinue!=1||!std::isfinite(v.altitude)||v.altitude<0||v.altitude>100||
           std::abs(static_cast<long long>(v.latitude_e7))>900000000LL||std::abs(static_cast<long long>(v.longitude_e7))>1800000000LL||
           !std::isfinite(v.params[0])||v.params[0]<0||v.params[0]>60||
           v.command!=(i==0?22:i+1==items.size()?21:16))return false;
    }
    expected_=std::move(items);received_.clear();offered_=0;plan_id_=0;started_=now;phase_=Phase::Upload;
    const auto op=status_.operation_id+1;status_={};status_.operation_id=op;status_.busy=true;status_.total=expected_.size();status_.state="uploading";status_.message="Dang tai mission; cho PX4 yeu cau waypoint.";emit();
    return transmit(build_mission_count(cfg_.gcs,cfg_.target_system,cfg_.target_component,static_cast<std::uint16_t>(expected_.size())),now);
}
void MissionManager::begin_read(MonoNs now){
    phase_=Phase::Count;received_.clear();download_count_=0;status_.busy=true;status_.verified=false;status_.next=0;
    status_.state="reading";status_.message="Dang doc lai mission tu PX4 de doi chieu.";emit();
    transmit(build_mission_request_list(cfg_.gcs,cfg_.target_system,cfg_.target_component),now);
}
bool MissionManager::readback(MonoNs now){if(status_.busy)return false;started_=now;++status_.operation_id;begin_read(now);return status_.busy;}
void MissionManager::on_packet(const FrameHeader& header,const MissionPacket& p,MonoNs now){
    if(header.sysid!=cfg_.target_system||header.compid!=cfg_.target_component||p.mission_type!=0)return;
    if(p.msgid==42){
        status_.current=p.seq;
        if(status_.verified&&p.count&&p.count!=expected_.size())invalidate("So muc mission PX4 da thay doi; can tai/xac minh lai.");
        if(status_.verified&&plan_id_&&p.opaque_id&&plan_id_!=p.opaque_id)invalidate("Mission ID PX4 da thay doi; can tai/xac minh lai.");
        return;
    }
    if(p.msgid==46){status_.reached=p.seq;return;}
    const bool target=(p.target_system==cfg_.gcs.sysid&&p.target_component==cfg_.gcs.compid);
    const bool ack_target=p.msgid==47&&(p.target_system==0||p.target_system==cfg_.gcs.sysid)&&(p.target_component==0||p.target_component==cfg_.gcs.compid);
    if(!target&&!ack_target)return;
    if(!status_.busy){if(p.msgid==44||p.msgid==73||p.msgid==39)invalidate("Phat hien giao dich mission ngoai thao tac hien tai; can doi chieu lai.");return;}
    if(p.msgid==47&&p.result!=0){fail("PX4 tu choi mission, result="+std::to_string(p.result));return;}
    if(phase_==Phase::Upload){
        if(p.msgid==40||p.msgid==51){
            if(p.seq>offered_||p.seq>=expected_.size())return;
            const bool progress=p.seq==offered_;if(progress)++offered_;
            status_.next=offered_;status_.message="Dang tai "+std::to_string(offered_)+"/"+std::to_string(expected_.size())+" muc.";
            transmit(build_mission_item(cfg_.gcs,cfg_.target_system,cfg_.target_component,expected_[p.seq]),now,progress);emit();
        }else if(p.msgid==47&&p.result==0&&offered_==expected_.size()){plan_id_=p.opaque_id;begin_read(now);}
    }else if(phase_==Phase::Count&&p.msgid==44){
        if(plan_id_&&p.opaque_id&&plan_id_!=p.opaque_id){fail("Mission ID thay doi trong luc doc lai.");return;}
        if(p.opaque_id)plan_id_=p.opaque_id;
        if(p.count>cfg_.max_items||(!expected_.empty()&&p.count!=expected_.size())){fail("Noi dung PX4 khac ban da tai (so muc).");return;}
        download_count_=p.count;status_.total=p.count;phase_=Phase::Items;
        if(p.count==0){transmit(build_mission_ack(cfg_.gcs,cfg_.target_system,cfg_.target_component,0),now);if(!status_.busy)return;phase_=Phase::Idle;status_.busy=false;status_.state="downloaded";status_.message="PX4 khong co mission.";emit();}
        else transmit(build_mission_request(cfg_.gcs,cfg_.target_system,cfg_.target_component,0),now);
    }else if(phase_==Phase::Items&&(p.msgid==73||p.msgid==39)){
        if(p.seq!=received_.size())return;
        if(!expected_.empty()&&!same_mission_item(expected_[p.seq],p.item)){fail("Mission doc lai khong khop tai seq="+std::to_string(p.seq));return;}
        received_.push_back(p.item);status_.next=received_.size();
        if(received_.size()==download_count_){
            if(!transmit(build_mission_ack(cfg_.gcs,cfg_.target_system,cfg_.target_component,0),now))return;
            phase_=Phase::Idle;status_.busy=false;status_.verified=!expected_.empty();status_.state=status_.verified?"verified":"downloaded";
            status_.message=status_.verified?"PX4 da nhan; doc lai va doi chieu mission thanh cong.":"Da doc mission PX4; can tai ban nhap cua web truoc khi Start.";
            last_.clear();emit();
        }else {transmit(build_mission_request(cfg_.gcs,cfg_.target_system,cfg_.target_component,static_cast<std::uint16_t>(received_.size())),now);emit();}
    }
}
void MissionManager::tick(MonoNs now){
    if(!status_.busy)return;
    if(now>=started_&&now-started_>120000000000ULL){fail("Mission vuot thoi gian giao dich; ket qua chua xac dinh.",true);return;}
    if(now>=last_sent_&&now-last_sent_>=cfg_.timeout_ns){
        if(attempts_>=cfg_.max_attempts){fail("Mission timeout; khong tu bat dau hay tai lai sau reconnect.",true);return;}
        transmit(last_,now,false);
    }
}
void MissionManager::disconnect(){if(status_.state=="failed"||status_.state=="unknown"){status_.verified=false;emit();return;}fail("Mat ket noi; mission tren PX4 can duoc doi chieu lai.",true);}
void MissionManager::invalidate(const std::string& why){if(status_.busy){fail(why,true);return;}status_.verified=false;status_.message=why;status_.state="invalidated";emit();}
bool MissionManager::consume_verified(){if(!status_.verified||status_.busy||status_.consumed)return false;status_.consumed=true;status_.verified=false;status_.state="start_requested";status_.message="Da yeu cau Start; doi ACK va trang thai PX4.";emit();return true;}
}
