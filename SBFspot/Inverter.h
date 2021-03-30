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

#include "osselect.h"

#include "InverterDataStorage.h"
#include "LiveData.h"
#include "SQLselect.h"
#include "mqtt.h"
#include "sma/EnergyMeter.h"

struct Config;
struct InverterData;

class Inverter
{
public:
    Inverter(const Config& config);
    ~Inverter();

    void exportConfig();
    int process(std::time_t timestamp);
    void reset();

private:
    int logOn();
    void logOff();

    bool dbOpen();
    void dbClose();

    int importSpotData(std::time_t timestamp);
    void importDayData();
    void importMonthData();
    void importEventData();

    void exportSpotData(std::time_t timestamp);
    void exportDayData();
    void exportMonthData();
    void exportEventData(const std::string& dt_range_csv);

    void exportSpotDataDb(std::time_t timestamp);
    void exportSpotDataMqtt(std::time_t timestamp);

    const Config& m_config;

    // TODO: transform this to a C++ container
    InverterData **m_inverters;
    InverterDataStorage m_storage;
    std::vector<DayStats>   m_dayStats;

#if defined(USE_SQLITE) || defined(USE_MYSQL)
    db_SQL_Export m_db;
#endif
    MqttExport m_mqtt;
    sma::EnergyMeter m_smaEnergyMeter;
    LiveData    m_smaEnergyMeterLiveData;
};

