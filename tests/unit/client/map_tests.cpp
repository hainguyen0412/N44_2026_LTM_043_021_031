#include "campus_map.hpp"
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <chrono>

#define CHECK(x) do{if(!(x))throw std::runtime_error(#x);}while(false)
int main(int argc,char** argv){
    try{
        CHECK(argc==2);webclient::CampusMap map(argv[1]);const auto& data=map.data();
        auto origin=map.to_geo({0,0,0});CHECK(std::abs(origin.latitude-16.074)<1e-10);CHECK(std::abs(origin.longitude-108.1499)<1e-10);
        for(auto p:std::vector<webclient::Local>{{0,0,0},{15,0,0},{0,15,0},{-307.5,432,20},{78,-18,0}}){const auto gps=map.to_geo(p);const auto back=map.to_local(gps);CHECK(std::abs(back.east-p.east)<.001);CHECK(std::abs(back.north-p.north)<.001);CHECK(std::abs(back.up-p.up)<.001);}
        CHECK(map.to_geo({15,0,0}).longitude>origin.longitude);CHECK(map.to_geo({0,15,0}).latitude>origin.latitude);
        const auto pixel=map.pixel(origin);CHECK(std::abs(pixel["x"].get<double>()-205)<.001);CHECK(std::abs(pixel["y"].get<double>()-288)<.001);
        nlohmann::json draft={{"schema_version",1},{"map_id",data["map_id"]},{"map_version",data["version"]},{"takeoff_altitude",10}};
        draft["points"]=nlohmann::json::array({
            {{"x",205},{"y",240},{"alt",10},{"hold",0}},{{"x",205},{"y",156},{"alt",10},{"hold",0}},
            {{"x",180},{"y",156},{"alt",10},{"hold",0}},{{"x",205},{"y",220},{"alt",10},{"hold",0}}
        });
        const auto items=map.compile(draft,{16.074,108.1499,10},100);CHECK(items.size()==7);CHECK(items[0].command==22);CHECK(items[6].command==21);CHECK(items[1].latitude_e7>items[0].latitude_e7);
        CHECK(items[5].command==16);CHECK(items[5].altitude==items[4].altitude);
        CHECK(items[5].latitude_e7==items[6].latitude_e7&&items[5].longitude_e7==items[6].longitude_e7);
        CHECK(items[5].params[0]==1&&items[5].params[1]==1);
        auto reject=[&](auto body){bool caught=false;try{map.compile(body,{16.074,108.1499,10},100);}catch(...){caught=true;}CHECK(caught);};
        auto bad=draft;bad["points"][0]["x"]=-1;reject(bad);
        bad=draft;bad["map_version"]="wrong";reject(bad);
        bad=draft;bad["points"]=nlohmann::json::array({{{"x",179},{"y",210},{"alt",10},{"hold",0}}});reject(bad);
        // The return leg crosses day_nha_phai_2: at 50 m it is clear,
        // at 10 m it remains blocked. LAND's 0 m applies only at Home.
        auto high=draft;high["takeoff_altitude"]=50;
        high["points"]=nlohmann::json::array({{{"x",250},{"y",180},{"alt",50},{"hold",0}}});
        const auto returning=map.compile(high,{16.074,108.1499,10},100);
        CHECK(returning.size()==4&&returning[2].command==16&&returning[2].altitude==50&&returning[3].altitude==0);
        bad=high;bad["takeoff_altitude"]=10;bad["points"][0]["alt"]=10;reject(bad);
        // Keep the allowed 2 m vertical Home movement inside the clearance budget.
        bad=high;bad["takeoff_altitude"]=16;bad["points"][0]["alt"]=16;reject(bad);
        bad=draft;bad["points"]=nlohmann::json::array();
        for(int i=0;i<97;++i)bad["points"].push_back({{"x",205},{"y",240},{"alt",10},{"hold",0}});
        CHECK(map.compile(bad,{16.074,108.1499,10},100).size()==100);
        bad["points"].push_back(bad["points"][0]);reject(bad);
        // Use actual Home for both the takeoff/landing column and mission GPS.
        const auto offset=map.to_geo({4.5,0,0});
        auto shifted=map.compile(draft,{offset.latitude,offset.longitude,10},100);
        CHECK(shifted[shifted.size()-2].longitude_e7==std::llround(offset.longitude*1e7));
        // Put an obstacle at the shifted Home but outside the gate's buffer:
        // a check using the fixed gate would incorrectly approve this landing.
        auto synthetic=data;
        synthetic["buildings"]=nlohmann::json::array({{{"name","Home obstacle"},{"x",207},{"y",287},{"w",2},{"h",2},{"roof_m",8}}});
        struct TemporaryMap {
            std::filesystem::path path=std::filesystem::temp_directory_path()/("mgw_map_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".json");
            ~TemporaryMap(){std::error_code e;std::filesystem::remove(path,e);}
        } temporary;
        {std::ofstream out(temporary.path);out<<synthetic.dump();}
        webclient::CampusMap blocked(temporary.path.string());
        bool refused=false;
        try{blocked.compile(high,{offset.latitude,offset.longitude,10},100);}catch(const std::invalid_argument&){refused=true;}
        CHECK(refused);
        std::cout<<"PASS coordinates, roundtrip, frame directions, mission compilation and invalid/obstacle inputs\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
