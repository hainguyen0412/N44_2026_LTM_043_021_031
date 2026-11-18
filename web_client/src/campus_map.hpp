#pragma once
#include "mavlink_gateway/mission.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace webclient {
struct Geo {double latitude,longitude,altitude;};
struct Local {double east,north,up;};
class CampusMap {
    nlohmann::json data_;
    double lat_,lon_,height_;
public:
    explicit CampusMap(const std::string& path);
    const nlohmann::json& data() const {return data_;}
    Geo to_geo(Local) const;
    Local to_local(Geo) const;
    nlohmann::json pixel(Geo) const;
    std::vector<mavlink_gateway::MissionItem> compile(const nlohmann::json&,const mavlink_gateway::HomePosition&,std::size_t maximum) const;
};
}
