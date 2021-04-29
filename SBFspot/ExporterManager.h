/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2021, SBF

    Latest version found at https://github.com/SBFspot/SBFspot

    License: Attribution-NonCommercial-ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)
    http://creativecommons.org/licenses/by-nc-sa/3.0/

    You are free:
        to Share - to copy, distribute and transmit the work
        to Remix - to adapt the work
    Under the following conditions:
    Attribution:
        You must attribute the work in the manner specified by the author or licensor
        (but not in any way that suggests that they endorse you or your use of the work).
    Noncommercial:
        You may not use this work for commercial purposes.
    Share Alike:
        If you alter, transform, or build upon this work, you may distribute the resulting work
        only under the same or similar license to this one.

DISCLAIMER:
    A user of SBFspot software acknowledges that he or she is receiving this
    software on an "as is" basis and the user is not relying on the accuracy
    or functionality of the software for any purpose. The user further
    acknowledges that any use of this software will be at his own risk
    and the copyright owner accepts no responsibility whatsoever arising from
    the use or application of the software.

    SMA is a registered trademark of SMA Solar Technology AG

************************************************************************************************/

#pragma once

#include <Exporter.h>
#include <json/JsonSerializer.h>
#include <msgpack/MsgPackSerializer.h>

class Cache;
struct Config;
class Storage;

class ExporterManager : public Exporter {
public:
    ExporterManager(const Config& config, Cache& cache);
    ~ExporterManager();

    bool init() override;
    bool open() override;
    void close() override;

    // New functions
    void exportConfig(const InverterData& inverterData) override;
    void exportLiveData(const LiveData& liveData) override;
    void exportDayData(const std::vector<DayData>& dayData) override;
    void exportMonthData(const std::vector<MonthData>& monthData) override;
    void exportDayStats(const DayStats& dayStats) override;

    // TODO: these functions shall be removed/replaced.
    void exportSpotData(std::time_t timestamp, const std::vector<InverterData>& inverters) override;
    void exportDayData(const std::vector<InverterData>& inverters) override;
    void exportMonthData(const std::vector<InverterData>& inverters) override;
    void exportEventData(const std::vector<InverterData>& inverters, const std::string& dt_range_csv) override;

    Storage* storage();

private:
    const Config&   m_config;
    Cache&          m_cache;
    Storage*        m_storage = nullptr;

    json::JsonSerializer m_jsonSerializer;
    msgpack::MsgPackSerializer m_msgPackSerializer;
    std::list<Exporter*> m_exporters;
};

