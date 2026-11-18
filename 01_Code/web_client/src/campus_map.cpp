#include "campus_map.hpp"
#include "home_reference.hpp"
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <algorithm>

namespace webclient {
using Json=nlohmann::json;
namespace {
constexpr double pi=3.14159265358979323846,a=6378137.0,e2=6.6943799901413165e-3;
struct XYZ {double x,y,z;};
XYZ ecef(Geo p){const double lat=p.latitude*pi/180,lon=p.longitude*pi/180,n=a/std::sqrt(1-e2*std::sin(lat)*std::sin(lat));return {(n+p.altitude)*std::cos(lat)*std::cos(lon),(n+p.altitude)*std::cos(lat)*std::sin(lon),(n*(1-e2)+p.altitude)*std::sin(lat)};}
double finite(const Json& j,const char* key,double low,double high){
    if(!j.contains(key)||!j[key].is_number())throw std::invalid_argument(std::string("Thieu/sai truong: ")+key);
    const double v=j[key].get<double>();if(!std::isfinite(v)||v<low||v>high)throw std::invalid_argument(std::string("Ngoai khoang: ")+key);return v;
}
bool crosses(double x0,double y0,double x1,double y1,double left,double top,double right,double bottom){
    double lo=0,hi=1;const double dx=x1-x0,dy=y1-y0;
    auto clip=[&](double p,double q){if(std::abs(p)<1e-12)return q>=0;const double t=q/p;if(p<0)lo=std::max(lo,t);else hi=std::min(hi,t);return lo<=hi;};
    return clip(-dx,x0-left)&&clip(dx,right-x0)&&clip(-dy,y0-top)&&clip(dy,bottom-y0);
}
}
CampusMap::CampusMap(const std::string& path){
    std::ifstream f(path);if(!f)throw std::runtime_error("Khong doc duoc campus_map.json");f>>data_;
    if(data_.at("schema_version")!=1||data_.at("frame")!="ENU")throw std::runtime_error("Map schema/frame khong duoc ho tro");
    finite(data_,"width",1,10000);finite(data_,"height",1,10000);finite(data_,"scale",.0001,100);
    lat_=finite(data_.at("origin"),"latitude",-89,89);lon_=finite(data_.at("origin"),"longitude",-180,180);height_=finite(data_.at("origin"),"elevation",-1000,10000);
    if(data_.at("origin").value("heading_deg",0.0)!=0)throw std::runtime_error("Map nay can heading ENU=0; chua hieu chuan map xoay");
}
Geo CampusMap::to_geo(Local p)const{
    const auto origin=ecef({lat_,lon_,height_});const double lat=lat_*pi/180,lon=lon_*pi/180;
    const double x=origin.x-std::sin(lon)*p.east-std::sin(lat)*std::cos(lon)*p.north+std::cos(lat)*std::cos(lon)*p.up;
    const double y=origin.y+std::cos(lon)*p.east-std::sin(lat)*std::sin(lon)*p.north+std::cos(lat)*std::sin(lon)*p.up;
    const double z=origin.z+std::cos(lat)*p.north+std::sin(lat)*p.up;
    const double radius=std::hypot(x,y);double phi=std::atan2(z,radius*(1-e2)),h=0;
    for(int i=0;i<12;++i){const double n=a/std::sqrt(1-e2*std::sin(phi)*std::sin(phi));h=radius/std::cos(phi)-n;phi=std::atan2(z,radius*(1-e2*n/(n+h)));}
    return {phi*180/pi,std::atan2(y,x)*180/pi,h};
}
Local CampusMap::to_local(Geo p)const{
    const auto origin=ecef({lat_,lon_,height_}),v=ecef(p);const double x=v.x-origin.x,y=v.y-origin.y,z=v.z-origin.z;
    const double lat=lat_*pi/180,lon=lon_*pi/180;
    return {-std::sin(lon)*x+std::cos(lon)*y,-std::sin(lat)*std::cos(lon)*x-std::sin(lat)*std::sin(lon)*y+std::cos(lat)*z,std::cos(lat)*std::cos(lon)*x+std::cos(lat)*std::sin(lon)*y+std::sin(lat)*z};
}
Json CampusMap::pixel(Geo p)const{const auto en=to_local(p);const double scale=data_.at("scale");return {{"x",data_.at("gate").at("x").get<double>()+en.east/scale},{"y",data_.at("gate").at("y").get<double>()-en.north/scale}};}
std::vector<mavlink_gateway::MissionItem> CampusMap::compile(const Json& draft,const mavlink_gateway::HomePosition& home,std::size_t maximum)const{
    if(draft.at("schema_version")!=1||draft.at("map_id")!=data_.at("map_id")||draft.at("map_version")!=data_.at("version"))throw std::invalid_argument("Ban nhap khac map/schema; can tao lai tren map hien tai.");
    const auto& points=draft.at("points");if(!points.is_array()||points.empty()||points.size()+3>maximum)throw std::invalid_argument("So waypoint khong hop le (can 3 muc cho TAKEOFF, ve tren Home, LAND).");
    const double width=data_.at("width"),height=data_.at("height"),scale=data_.at("scale"),gx=data_.at("gate").at("x"),gy=data_.at("gate").at("y");
    const auto home_local=to_local({home.latitude,home.longitude,height_});
    if(!cclient::valid_home(home)||std::hypot(home_local.east,home_local.north)>5)throw std::invalid_argument("Home PX4 khong hop le/cach goc cong hon 5 m; can hieu chuan map/spawn truoc.");
    const double hx=gx+home_local.east/scale,hy=gy-home_local.north/scale;
    const double takeoff=finite(draft,"takeoff_altitude",2,100);
    using mavlink_gateway::MissionItem;std::vector<MissionItem> result;
    auto item=[&](std::uint16_t cmd,double lat,double lon,double alt,double hold){MissionItem v;v.seq=static_cast<std::uint16_t>(result.size());v.command=cmd;v.current=result.empty()?1:0;v.latitude_e7=static_cast<std::int32_t>(std::llround(lat*1e7));v.longitude_e7=static_cast<std::int32_t>(std::llround(lon*1e7));v.altitude=static_cast<float>(alt);v.params[0]=static_cast<float>(hold);result.push_back(v);};
    item(22,home.latitude,home.longitude,takeoff,0);
    struct RoutePoint{double x,y,alt;};std::vector<RoutePoint> route{{hx,hy,0},{hx,hy,takeoff}};
    for(const auto& p:points){const double x=finite(p,"x",0,width),y=finite(p,"y",0,height),alt=finite(p,"alt",2,100),hold=finite(p,"hold",0,60);const auto geo=to_geo({(x-gx)*scale,(gy-y)*scale,0});item(16,geo.latitude,geo.longitude,alt,hold);route.push_back({x,y,alt});}
    const double approach_altitude=route.back().alt;
    route.push_back({hx,hy,approach_altitude});
    item(16,home.latitude,home.longitude,approach_altitude,1);
    result.back().params[1]=1.0f; // Stop above Home within a 1 m acceptance radius.
    route.push_back({hx,hy,0});item(21,home.latitude,home.longitude,0,0);
    // Conservative check for known building footprints. It is not an obstacle avoidance planner.
    for(std::size_t i=1;i<route.size();++i)for(const auto& b:data_.at("buildings")){
        const double x=b.at("x"),y=b.at("y"),w=b.at("w"),h=b.at("h"),margin=2/scale;
        const double roof=b.at("roof_m").get<double>()+(w*h*scale*scale>400?1.6:.9)+2;
        // Relative-altitude items move with Home. Reserve the full permitted
        // vertical Home change in addition to the existing roof clearance.
        const double lowest=std::min(route[i-1].alt,route[i].alt)+home.altitude_msl-height_-cclient::home_vertical_tolerance_m;
        if(lowest<=roof&&crosses(route[i-1].x,route[i-1].y,route[i].x,route[i].y,x-margin,y-margin,x+w+margin,y+h+margin))
            throw std::invalid_argument("Doan "+std::to_string(i)+" cat qua "+b.at("name").get<std::string>()+" o do cao chua du; doi duong bay/do cao.");
    }
    return result;
}
}
