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

#include "ArchData.h"
#include "Cache.h"
#include "ExporterManager.h"
#include "LiveData.h"
#include "SBFNet.h"

struct Config;
class Ethernet;
class Importer;
struct InverterData;
class SbfSpot;

class Inverter
{
public:
    Inverter(const Config& config, Ethernet& ethernet, Importer& import, SbfSpot& sbfSpot);
    ~Inverter();

    E_SBFSPOT ethInitConnection();
    E_SBFSPOT logonSMAInverter(std::vector<InverterData>& inverters, long userGroup, const char *password);
    E_SBFSPOT logoffSMAInverter(const InverterData& inverter);
    E_SBFSPOT logoffMultigateDevices(const std::vector<InverterData>& inverters);
    E_SBFSPOT getDeviceList(std::vector<InverterData>& inverters, int multigateID);
    int getInverterData(std::vector<InverterData>& inverters, SmaInverterDataSet type);

    void exportConfig();
    int process(std::time_t timestamp);
    void reset();

private:
    std::string discover();

    int logOn();
    void logOff();

    int importSpotData(std::time_t timestamp);
    void importDayData();
    void importMonthData();
    void importEventData();

    void CalcMissingSpot(InverterData& invData);

    const Config& m_config;
    Ethernet& m_ethernet;
    Importer& m_import;
    SbfSpot& m_sbfSpot;
    Buffer  m_buffer;

    std::vector<InverterData> m_inverters;
    ArchData m_archData;
    Cache m_cache;
    std::vector<DayStats>   m_dayStats;

    ExporterManager m_exporterManager;
};

