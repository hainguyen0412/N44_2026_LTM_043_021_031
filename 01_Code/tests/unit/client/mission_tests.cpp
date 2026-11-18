#include "mission_manager.hpp"
#include "command_manager.hpp"
#include "home_reference.hpp"
#include "mavlink_gateway/processing.hpp"
#include <iostream>
#include <stdexcept>

using namespace cclient;
using namespace mavlink_gateway;
#define CHECK(expr) do{if(!(expr))throw std::runtime_error(#expr);}while(false)
namespace {
std::vector<MissionItem> plan(){std::vector<MissionItem> v(3);for(int i=0;i<3;++i){v[i].seq=static_cast<std::uint16_t>(i);v[i].command=i==0?22:i==2?21:16;v[i].latitude_e7=160740000;v[i].longitude_e7=1081499000;v[i].altitude=i==2?0.f:10.f;}v[0].current=1;return v;}
MissionPacket packet(unsigned id,unsigned seq=0){MissionPacket p;p.msgid=id;p.seq=static_cast<std::uint16_t>(seq);p.target_system=254;p.target_component=190;return p;}
FrameHeader source(){FrameHeader h;h.sysid=1;h.compid=1;return h;}
MissionConfig config(){MissionConfig c;c.gcs={254,190};c.timeout_ns=100;c.max_attempts=2;return c;}
unsigned msgid(const Bytes& raw){return raw[7]|(unsigned(raw[8])<<8)|(unsigned(raw[9])<<16);}
void happy(){
    std::vector<Bytes> sent;MissionManager m(config(),[&](const Bytes& b){sent.push_back(b);return true;});
    CHECK(m.upload(plan(),1));CHECK(msgid(sent.back())==44);
    auto wrong=source();wrong.sysid=2;m.on_packet(wrong,packet(51),2);CHECK(sent.size()==1);
    m.on_packet(source(),packet(51,2),2);CHECK(sent.size()==1);
    m.on_packet(source(),packet(47),3);CHECK(m.status().state=="uploading");
    for(unsigned i=0;i<3;++i){m.on_packet(source(),packet(i==0?40:51,i),4+i);CHECK(msgid(sent.back())==73);}
    CHECK(m.status().next==3);m.on_packet(source(),packet(47),8);CHECK(msgid(sent.back())==43);
    auto count=packet(44);count.count=3;m.on_packet(source(),count,9);CHECK(msgid(sent.back())==51);
    const auto items=plan();for(unsigned i=0;i<3;++i){auto p=packet(73,i);p.item=items[i];p.item.frame=3;m.on_packet(source(),p,10+i);}
    CHECK(m.status().verified);CHECK(!m.status().busy);CHECK(msgid(sent.back())==47);
    CHECK(m.consume_verified());CHECK(!m.consume_verified());CHECK(m.status().consumed);
    m.disconnect();CHECK(!m.status().verified);CHECK(m.status().state=="unknown");
}
void duplicate_request(){
    std::vector<Bytes> sent;MissionManager m(config(),[&](const Bytes& b){sent.push_back(b);return true;});CHECK(m.upload(plan(),1));
    m.on_packet(source(),packet(51,0),2);m.on_packet(source(),packet(51,0),3);
    CHECK(m.status().next==1);CHECK(sent.size()==3);CHECK(msgid(sent[2])==73);
}
void timeout_and_send_error(){
    std::vector<Bytes> sent;MissionManager m(config(),[&](const Bytes& b){sent.push_back(b);return true;});CHECK(m.upload(plan(),1));
    m.tick(101);CHECK(sent.size()==2);m.tick(201);CHECK(!m.status().busy);CHECK(m.status().state=="unknown");
    MissionManager failed(config(),[](const Bytes&){return false;});CHECK(!failed.upload(plan(),1));CHECK(failed.status().state=="unknown");
}
void mismatch(){
    MissionManager m(config(),[](const Bytes&){return true;});CHECK(m.upload(plan(),1));
    for(unsigned i=0;i<3;++i)m.on_packet(source(),packet(51,i),2+i);
    m.on_packet(source(),packet(47),5);auto count=packet(44);count.count=3;m.on_packet(source(),count,6);
    auto p=packet(73);p.item=plan()[0];p.item.altitude=99;m.on_packet(source(),p,7);
    CHECK(!m.status().verified);CHECK(m.status().state=="failed");
}
void invalidation_and_targets(){
    MissionManager m(config(),[](const Bytes&){return true;});CHECK(m.upload(plan(),1));
    auto bad=packet(51);bad.target_system=255;m.on_packet(source(),bad,2);CHECK(m.status().next==0);
    bad=packet(51);bad.mission_type=1;m.on_packet(source(),bad,3);CHECK(m.status().next==0);
    m.invalidate("home changed");CHECK(m.status().state=="unknown");CHECK(!m.status().busy);
    auto invalid=plan();invalid[1].frame=5;CHECK(!m.upload(invalid,4));
}
void wire(){
    Parser parser;ReceiveEvent ev;ev.source={1,1,Transport::Tcp,Flow::ControlUplink,{"127.0.0.1",1}};
    ev.bytes=build_mission_item({254,190},1,1,plan()[1]);auto batch=parser.consume(ev);CHECK(batch.frames.size()==1);CHECK(batch.issues.empty());
    const auto p=decode_mission(batch.frames[0]);CHECK(p);CHECK(p->target_system==1);CHECK(p->item.latitude_e7==160740000);CHECK(p->item.longitude_e7==1081499000);CHECK(same_mission_item(p->item,plan()[1]));
    ev.bytes[10]^=1;batch=parser.consume(ev);CHECK(batch.frames.empty());CHECK(!batch.issues.empty());
}
void start_does_not_retry(){
    CommandConfig cfg;cfg.ack_timeout_ns=100;cfg.max_sends=3;unsigned sent=0;
    CommandManager command(cfg,[](std::uint16_t,const Params&,std::uint8_t){return Bytes{1};},[&](const Bytes&){++sent;return true;},{});
    CHECK(command.submit(kCmdMissionStart,{},1));command.tick(101);command.tick(1001);
    CHECK(sent==1);CHECK(!command.busy());CHECK(command.last_outcome()==Outcome::Unknown);
}
void home_reference_limits(){
    const HomePosition origin{16.074,108.1499,10};
    auto current=origin;current.latitude+=2e-7;current.altitude_msl+=.7;
    CHECK(!compare_home(origin,current).changed());
    CHECK(compare_home(origin,current).horizontal_m>.02);
    current=origin;current.latitude+=.9/111195.0;CHECK(!compare_home(origin,current).changed());
    current.latitude+=.2/111195.0;CHECK(compare_home(origin,current).changed());
    current=origin;current.latitude+=.8/111195.0;current.longitude+=.8/(111195.0*std::cos(origin.latitude*3.141592653589793/180));
    CHECK(compare_home(origin,current).changed()); // Radial distance, not an axis-aligned degree box.
    current=origin;current.altitude_msl+=1.99;CHECK(!compare_home(origin,current).changed());
    current.altitude_msl+=.02;CHECK(compare_home(origin,current).changed());
    current=origin;current.latitude=std::numeric_limits<double>::quiet_NaN();CHECK(compare_home(origin,current).changed());
    CHECK(compare_home({0,179.999999,0},{0,-179.999999,0}).horizontal_m<.3);
}
}
int main(){unsigned passed=0;for(const auto& test:std::vector<std::pair<const char*,void(*)()>>{{"happy",happy},{"duplicate_request",duplicate_request},{"timeout_send_error",timeout_and_send_error},{"mismatch",mismatch},{"invalidation_targets",invalidation_and_targets},{"wire_crc",wire},{"start_does_not_retry",start_does_not_retry},{"home_reference_limits",home_reference_limits}}){try{test.second();++passed;std::cout<<"PASS "<<test.first<<'\n';}catch(const std::exception& e){std::cerr<<"FAIL "<<test.first<<": "<<e.what()<<'\n';}}return passed==8?0:1;}
