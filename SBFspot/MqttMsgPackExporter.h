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

#include "Exporter.h"

#include <chrono>
#include <mosquittopp.h>
#include <msgpack.hpp>

struct Config;
struct InverterData;
struct StringConfig;

// TODO: implement static plugin registration: https://dxuuu.xyz/cpp-static-registration.html
class MqttMsgPackExport : public Exporter, mosqpp::mosquittopp
{
public:
    MqttMsgPackExport(const Config& config);
    ~MqttMsgPackExport();

    std::string name() const override;

    void connectToHost();
    void disconnectFromHost();

    void exportConfig(const InverterData& inverterData) override;
    void exportDayStats(const DayStats& dayStats) override;
    void exportLiveData(const LiveData& emeterData) override;

    void exportDayData(std::time_t timestamp, const DataPerInverter& inverterData) override;

private:
    void publish(const std::string& topic, const msgpack::sbuffer& buffer, uint8_t qos = 0);

    void on_connect(int rc) override;
    void on_disconnect(int rc) override;

    const Config& m_config;
    std::multimap<uint32_t, StringConfig> m_arrayConfig;

    bool m_isConnected = false;
    std::vector<float> m_powerMaxToday;
};
