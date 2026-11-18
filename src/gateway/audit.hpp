#pragma once
#include "mavlink_gateway/config.hpp"
#include "mavlink_gateway/csv_logger.hpp"
#include <memory>

namespace mavlink_gateway {
std::string csv_row(const std::vector<std::string>& values);
// A supplies network records; B owns queued CSV I/O.
class Audit {
    std::unique_ptr<CsvLogger> logger_;
public:
    std::filesystem::path directory;
    explicit Audit(const Config&);
    void log(const std::string& file,std::vector<std::string> values,bool detail=false);
    void close(){if(logger_)logger_->close();}
    std::uint64_t dropped() const{return logger_?logger_->stats().dropped:0;}
    bool truncated() const{return logger_&&logger_->stats().log_truncated;}
    bool failed() const{return logger_&&!logger_->stats().last_error.empty();}
};
}
