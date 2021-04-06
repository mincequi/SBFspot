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

#include <cstdint>
#include <ctime>
#include <set>

#include "Types.h"

class Config;
class Ethernet_qt;
class QNetworkDatagram;

namespace sma {

class SmaInverter
{
public:
    SmaInverter(const Config& config, Ethernet_qt& ioDevice, uint32_t address);

    void poll();

private:
    void init();
    void login();
    void logout();
    void requestData();
    void requestDataSet(SmaInverterDataSet dataSet);
    void exportData();

    void onDatagram(const QNetworkDatagram& datagram);

    const Config&   m_config;
    Ethernet_qt&    m_ioDevice;

    uint32_t m_address = 0;
    uint16_t m_susyId = 0x0078;
    uint32_t m_serial = 0x3803E8C8;
    std::time_t m_lastSeen = 0;

    enum class State {
        Invalid,
        Initialized,
        LoggedIn
    } m_state = State::Invalid;

    std::set<LriDef> m_pendingLris;
    InverterData     m_pendingData;
    InverterDataMap  m_pendingDataMap;

    friend class SmaManager;
};

}