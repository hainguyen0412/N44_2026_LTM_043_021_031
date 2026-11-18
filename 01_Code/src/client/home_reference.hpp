#pragma once
#include "mavlink_gateway/mission.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace cclient {
// Used by both compile-to-queue and live mission reference checks.
constexpr double home_horizontal_tolerance_m = 1.0;
constexpr double home_vertical_tolerance_m = 2.0;
inline bool valid_home(const mavlink_gateway::HomePosition& h) {
    return std::isfinite(h.latitude) && std::isfinite(h.longitude) &&
           std::isfinite(h.altitude_msl) && std::abs(h.latitude)<=90 && std::abs(h.longitude)<=180;
}
struct HomeDelta {
    double horizontal_m, vertical_m;
    bool changed() const {
        return horizontal_m>home_horizontal_tolerance_m || vertical_m>home_vertical_tolerance_m;
    }
    std::string description() const {
        std::ostringstream text;
        text<<std::fixed<<std::setprecision(2)<<"ngang="<<horizontal_m<<" m, cao="<<vertical_m<<" m";
        return text.str();
    }
};
inline HomeDelta compare_home(const mavlink_gateway::HomePosition& reference,
                              const mavlink_gateway::HomePosition& current) {
    if(!valid_home(reference)||!valid_home(current))
        return {std::numeric_limits<double>::infinity(),std::numeric_limits<double>::infinity()};
    constexpr double rad=3.14159265358979323846/180.0, radius=6371008.8;
    const double dlat=(current.latitude-reference.latitude)*rad;
    const double dlon=std::remainder(current.longitude-reference.longitude,360.0)*rad;
    const double q=std::pow(std::sin(dlat/2),2)+
        std::cos(reference.latitude*rad)*std::cos(current.latitude*rad)*std::pow(std::sin(dlon/2),2);
    return {2*radius*std::asin(std::sqrt(std::clamp(q,0.0,1.0))),
            std::abs(current.altitude_msl-reference.altitude_msl)};
}
} // namespace cclient
