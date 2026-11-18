#include "client_runtime.hpp"
#include "campus_map.hpp"
#include <httplib/httplib.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <sstream>
#include <unordered_map>
#include <windows.h>

using Json=nlohmann::json;
namespace fs=std::filesystem;
namespace {
std::atomic<bool> stop_requested{false};
BOOL WINAPI stop_handler(DWORD type){if(type==CTRL_C_EVENT||type==CTRL_BREAK_EVENT){stop_requested=true;return TRUE;}return FALSE;}
std::string random_token(){std::random_device rng;std::ostringstream s;for(int i=0;i<8;++i)s<<std::hex<<std::setw(8)<<std::setfill('0')<<rng();return s.str();}
void json_response(httplib::Response& response,const Json& value,int status=200){response.status=status;response.set_content(value.dump(),"application/json; charset=utf-8");response.set_header("Cache-Control","no-store");}
void identifier(const std::string& value){if(value.empty()||value.size()>80||value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")!=std::string::npos)throw std::invalid_argument("request_id khong hop le.");}
}
int main(int argc,char** argv){
    try{
        unsigned port=8080;std::string stop_file,asset_arg;std::vector<std::string> arguments{argv[0]};bool supplied_config=false;
        for(int i=1;i<argc;++i){std::string key=argv[i];if(key=="--web-port"||key=="--web-root"||key=="--stop-file"){
            if(++i>=argc)throw std::invalid_argument("Thieu gia tri "+key);
            if(key=="--web-port"){const std::string value=argv[i];if(value.empty()||value.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("web-port khong hop le");port=static_cast<unsigned>(std::stoul(value));if(!port||port>65535)throw std::invalid_argument("web-port ngoai khoang");}
            else if(key=="--web-root")asset_arg=argv[i];else stop_file=argv[i];
        }else{if(key=="--config")supplied_config=true;arguments.push_back(key);}}
        auto load=[&]{std::vector<char*> args;for(auto& a:arguments)args.push_back(a.data());return cclient::load_client_options(static_cast<int>(args.size()),args.data());};
        auto options=load();
        if(options.help){std::cout<<cclient::client_usage()<<"\nWeb: --web-port 8080 --web-root DIR --stop-file FILE\nNo TCP connection or flight commands until explicitly requested in the browser.\n";return 0;}
        if(!supplied_config){arguments.push_back("--config");arguments.push_back((options.project_root/"config/gateway.mission.example.json").string());options=load();}
        if(options.print_config){std::cout<<options.resolved_config_json<<'\n';return 0;}
        const fs::path assets=fs::weakly_canonical(asset_arg.empty()?options.project_root/"web_client/frontend":fs::absolute(asset_arg));
        if(!fs::is_regular_file(assets/"index.html"))throw std::runtime_error("Thieu frontend/index.html; giu web_client trong goi project.");
        webclient::CampusMap campus((assets/"assets/campus_map.json").string());
        cclient::ClientRuntime runtime(options);
        httplib::Server server;server.new_task_queue=[] {return new httplib::ThreadPool(8,32);};
        server.set_payload_max_length(65536);server.set_read_timeout(3,0);server.set_write_timeout(2,0);server.set_keep_alive_timeout(2);
        const std::string origin="http://127.0.0.1:"+std::to_string(port),localhost="http://localhost:"+std::to_string(port),csrf=random_token();
        std::mutex control_mutex;std::string owner;
        auto last_lease=std::chrono::steady_clock::now();
        const auto expire=[&]{if(!owner.empty()&&std::chrono::steady_clock::now()-last_lease>std::chrono::seconds(15))owner.clear();};
        const auto owned=[&](const httplib::Request& request){expire();return !owner.empty()&&request.get_header_value("X-Controller-Token")==owner;};
        const auto snapshot=[&]{
            auto s=runtime.snapshot();const auto& t=s["telemetry"];
            const auto map_pixel=[&](const Json& p){return campus.pixel({p["latitude"],p["longitude"],campus.data()["origin"]["elevation"]});};
            s["map_position"]=t.contains("position")?map_pixel(t["position"]):Json(nullptr);
            const auto& mission=s["mission"];
            const bool bound=mission.value("busy",false)||mission.value("verified",false)||mission.value("consumed",false);
            s["map_home"]=bound&&s.contains("mission_home")?map_pixel(s["mission_home"]):
                t.contains("home")?map_pixel(t["home"]):Json(nullptr);
            return s;
        };
        server.set_pre_routing_handler([&](const httplib::Request& req,httplib::Response& res){
            res.set_header("X-Content-Type-Options","nosniff");res.set_header("Referrer-Policy","no-referrer");
            res.set_header("Content-Security-Policy","default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; object-src 'none'; base-uri 'none'; frame-ancestors 'none'");
            const auto host=req.get_header_value("Host"),from=req.get_header_value("Origin");
            if((host!="127.0.0.1:"+std::to_string(port)&&host!="localhost:"+std::to_string(port))||(!from.empty()&&from!=origin&&from!=localhost)){
                json_response(res,{{"error","origin_denied"},{"message","Chi cho phep web local cung origin."}},403);return httplib::Server::HandlerResponse::Handled;
            }
            if(req.method=="POST"&&(req.get_header_value("X-CSRF-Token")!=csrf||req.get_header_value("Content-Type").find("application/json")==std::string::npos)){
                json_response(res,{{"error","csrf_required"},{"message","Thieu token local/JSON; tai lai trang web."}},403);return httplib::Server::HandlerResponse::Handled;
            }
            return httplib::Server::HandlerResponse::Unhandled;
        });
        server.Get("/api/bootstrap",[&](const auto&,auto& res){json_response(res,{{"csrf",csrf},{"map_version",campus.data()["version"]},{"gateway_config",options.config_path.string()},{"gateway_host",options.host},{"max_waypoints",options.shared.mission_max_items>3?options.shared.mission_max_items-3:0}});});
        server.Get("/api/map",[&](const auto&,auto& res){json_response(res,campus.data());});
        server.Get("/api/state",[&](const auto&,auto& res){json_response(res,snapshot());});
        std::atomic<int> subscribers{0};
        server.Get("/api/events",[&](const auto&,auto& res){
            if(subscribers.fetch_add(1)>=3){--subscribers;json_response(res,{{"error","subscriber_limit"}},429);return;}
            res.set_header("Cache-Control","no-cache");res.set_header("X-Accel-Buffering","no");
            res.set_chunked_content_provider("text/event-stream",[&](std::size_t,httplib::DataSink& sink){
                if(stop_requested||!sink.is_writable())return false;
                try{const auto data="data: "+snapshot().dump()+"\n\n";if(!sink.write(data.data(),data.size()))return false;std::this_thread::sleep_for(std::chrono::milliseconds(200));return true;}
                catch(...){return false;}
            },[&](bool){--subscribers;});
        });
        server.Post("/api/connect",[&](const auto& req,auto& res){
            try{
                const auto body=Json::parse(req.body);const auto role=body.at("role").template get<std::string>();if(role!="observer"&&role!="controller")throw std::invalid_argument("Vai tro khong hop le.");
                std::lock_guard<std::mutex> control(control_mutex);expire();auto current=runtime.snapshot();
                if(!owner.empty()&&!owned(req)){json_response(res,{{"error","controller_busy"},{"message","Tab khac dang giu quyen dieu khien; dong/ngat tab do hoac cho lease het han."}},409);return;}
                if(!current.value("connected",false)||current.value("role","")!=role){std::string error;if(!runtime.connect(role=="controller",error)){json_response(res,{{"error","gateway_connect_failed"},{"message",error}},503);return;}}
                if(role=="controller"){if(owner.empty())owner=random_token();last_lease=std::chrono::steady_clock::now();}else owner.clear();
                json_response(res,{{"connected",true},{"controller_token",role=="controller"?owner:""}});
            }catch(const std::exception& e){json_response(res,{{"error","invalid_request"},{"message",e.what()}},400);}
        });
        server.Post("/api/lease",[&](const auto& req,auto& res){std::lock_guard<std::mutex> lock(control_mutex);if(!owned(req)){json_response(res,{{"error","controller_owner_required"},{"message","Quyen dieu khien het han/thuoc tab khac. Nhan quyen lai bang nut Ket noi."}},403);return;}last_lease=std::chrono::steady_clock::now();json_response(res,{{"ok",true}});});
        server.Post("/api/disconnect",[&](const auto& req,auto& res){std::lock_guard<std::mutex> lock(control_mutex);const auto s=runtime.snapshot();if(s.value("connected",false)&&s.value("role","")=="controller"&&!owned(req)){json_response(res,{{"error","controller_owner_required"},{"message","Tab nay khong so huu controller."}},403);return;}runtime.disconnect();owner.clear();json_response(res,{{"connected",false}});});
        std::unordered_map<std::string,std::pair<std::string,Json>> operations;std::deque<std::string> operation_order;
        auto operation=[&](const std::string& route,auto action){server.Post(route,[&,action](const httplib::Request& req,httplib::Response& res){
            try{
                std::lock_guard<std::mutex> control(control_mutex);if(!owned(req)){json_response(res,{{"error","controller_owner_required"},{"message","Can quyen controller cua tab nay."}},403);return;}last_lease=std::chrono::steady_clock::now();
                auto body=Json::parse(req.body);const std::string id=body.at("request_id").get<std::string>();identifier(id);
                const std::string key=owner+":"+id,signature=req.path+"\n"+body.dump();const auto old=operations.find(key);
                if(old!=operations.end()){if(old->second.first!=signature){json_response(res,{{"error","request_id_conflict"},{"message","request_id da dung cho noi dung khac."}},409);return;}json_response(res,old->second.second,202);return;}
                action(body,id);Json result={{"accepted",true},{"request_id",id},{"message","Da tiep nhan yeu cau; theo doi snapshot de xem ket qua PX4."}};
                operations.emplace(key,std::make_pair(signature,result));operation_order.push_back(key);while(operation_order.size()>256){operations.erase(operation_order.front());operation_order.pop_front();}json_response(res,result,202);
            }catch(const std::exception& e){json_response(res,{{"error","request_failed"},{"message",e.what()}},400);}
        });};
        operation("/api/commands",[&](const Json& j,const std::string& id){runtime.command(j.at("command").get<std::string>(),id);});
        auto compile=[&](Json& j){const auto s=runtime.snapshot();if(!s.at("telemetry").contains("home"))throw std::runtime_error("Chua co HOME_POSITION. Bat stream HOME_POSITION tren PX4.");const auto h=s["telemetry"]["home"];j["_home"]={{"latitude",h["latitude"]},{"longitude",h["longitude"]},{"altitude_msl",h["altitude_msl"]}};return campus.compile(j,{h["latitude"],h["longitude"],h["altitude_msl"]},options.shared.mission_max_items);};
        operation("/api/missions/upload",[&](const Json& body,const std::string& id){auto j=body;auto items=compile(j);runtime.upload(std::move(items),std::move(j),id);});
        operation("/api/missions/readback",[&](const Json&,const std::string& id){runtime.readback(id);});
        operation("/api/missions/start",[&](const Json& body,const std::string& id){runtime.start_mission(id,body.at("upload_request_id").get<std::string>());});
        server.Post("/api/missions/validate",[&](const auto& req,auto& res){try{auto j=Json::parse(req.body);const auto items=compile(j);json_response(res,{{"valid",true},{"items",items.size()},{"map_version",campus.data()["version"]}});}catch(const std::exception& e){json_response(res,{{"error","invalid_mission"},{"message",e.what()}},400);}});
        server.Get(R"(/api/operations/([A-Za-z0-9_-]{1,80}))",[&](const auto& req,auto& res){
            std::lock_guard<std::mutex> lock(control_mutex);
            if(!owned(req)){json_response(res,{{"error","controller_owner_required"}},403);return;}
            const std::string id=req.matches[1];
            const auto found=operations.find(owner+":"+id);
            if(found==operations.end()){json_response(res,{{"error","operation_not_found"}},404);return;}
            const auto s=snapshot();auto result=found->second.second;
            result["command"]=s["command"].value("http_request_id","")==id?s["command"]:Json(nullptr);
            result["mission"]=s["mission"].value("request_id","")==id||s["mission"].value("upload_request_id","")==id?s["mission"]:Json(nullptr);
            result["superseded"]=result["command"].is_null()&&result["mission"].is_null();
            json_response(res,result);
        });
        const std::set<std::string> static_files{"/","/index.html","/styles.css","/app.js","/assets/campus_map.json","/assets/campus_2d.svg"};
        server.Get(R"(/.*)",[&](const auto& req,auto& res){if(!static_files.count(req.path)){json_response(res,{{"error","not_found"}},404);return;}const auto file=assets/(req.path=="/"?"index.html":req.path.substr(1));std::ifstream input(file,std::ios::binary);if(!input){json_response(res,{{"error","asset_missing"}},404);return;}std::ostringstream bytes;bytes<<input.rdbuf();const auto ext=file.extension().string();res.set_content(bytes.str(),ext==".html"?"text/html; charset=utf-8":ext==".css"?"text/css; charset=utf-8":ext==".js"?"text/javascript; charset=utf-8":ext==".svg"?"image/svg+xml":"application/json; charset=utf-8");res.set_header("Cache-Control","no-cache");});
        if(!server.bind_to_port("127.0.0.1",static_cast<int>(port)))throw std::runtime_error("Khong bind duoc HTTP port; kiem tra cong dang dung.");
        SetConsoleCtrlHandler(stop_handler,TRUE);std::atomic<bool> finished{false};
        std::thread monitor([&]{while(!finished){if(stop_requested||(!stop_file.empty()&&fs::exists(stop_file))){stop_requested=true;server.stop();break;}std::this_thread::sleep_for(std::chrono::milliseconds(100));}});
        std::cout<<"WEB READY "<<origin<<"\nConfig: "<<options.config_path.string()<<"\nAssets: "<<assets.string()<<"\nFile logging: "<<(options.shared.logging_enabled?"ON":"OFF")<<"\nMo URL tren trinh duyet. Gateway/PX4 khong duoc tu khoi dong. Ctrl+C de dung.\n"<<std::flush;
        const bool ok=server.listen_after_bind();finished=true;stop_requested=true;if(monitor.joinable())monitor.join();runtime.disconnect();
        if(!ok){std::cerr<<"Khong bind duoc HTTP 127.0.0.1:"<<port<<"; kiem tra cong dang dung.\n";return 1;}return 0;
    }catch(const std::exception& e){std::cerr<<"Web client error: "<<e.what()<<'\n';return 2;}
}
