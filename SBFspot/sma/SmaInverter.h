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

#include <QObject>

#include "LiveData.h"
#include "SBFspot.h"
#include "Types.h"

class Config;
class Ethernet_qt;
class Storage;
class QNetworkDatagram;

namespace sma {

class SmaInverter : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief The current inverter state.
     */
    enum class State {
        Invalid,
        Initialized,
        LoggedIn
    };

    /**
     * @brief SmaInverter
     * @param parent
     * @param config
     * @param ioDevice
     * @param address
     * @param storage
     */
    SmaInverter(QObject* parent, const Config& config, Ethernet_qt& ioDevice, uint32_t address, Storage* storage);

    // TODO: these are candidates for std::future and std::promise
    /**
     * @brief Log in to inverter.
     * @param timestamp for upcoming request
     */
    void login(std::time_t timestamp);

    /**
     * @brief Log out from inverter.
     */
    void logout();

    /**
     * @brief Request LiveData from inverter asynchronously. The result can be obtained via result().
     */
    void requestLiveData();

    /**
     * @brief Request DayData from inverter asynchronously. The result can be obtained via result().
     * @param from timestamp to start from
     * @param to timestamp to end
     */
    void requestDayData(std::time_t from, std::time_t to);

    /**
     * @brief Request MonthData from inverter asynchronously. The result can be obtained via result().
     * @param from timestamp to start from
     * @param to timestamp to end
     */
    void requestMonthData(std::time_t from, std::time_t to);

    /**
     * @brief Obtain result(s) from a previous called requestXxxData().
     * @return A list of responses. One item for each data set.
     */
    std::list<SmaResponse> result();

signals:
    /**
     * @brief Notifies about the current inverter state.
     * @param state
     */
    void stateChanged(State state);

private:
    /**
     * @brief Initialize inverter.
     */
    void init();

    void resetPendingData();
    void requestDataSet(SmaInverterDataSet dataSet);

    void onDatagram(const QNetworkDatagram& datagram);

    void decodeResponse(ByteBuffer& buffer, InverterDataMap& inverterDataMap, std::set<LriDef>& lris);
    void decodeDayData(const ByteBuffer& buffer);
    void decodeMonthData(const ByteBuffer& buffer);

    const Config&   m_config;
    Ethernet_qt&    m_ioDevice;
    Storage*        m_storage = nullptr;
    SbfSpot         m_sbfSpot;

    uint32_t m_address = 0;
    uint16_t m_susyId = 0x0078;
    Serial m_serial = 0x3803E8C8;
    std::time_t m_lastSeen = 0;

    State m_state = State::Invalid;

    std::set<LriDef>    m_pendingLris;
    LiveData            m_pendingLiveData;
    std::vector<DayData>  m_pendingDayData;
    std::vector<MonthData> m_pendingMonthData;
    // TODO: just an experiment.
    InverterDataMap     m_pendingDataMap;

    friend class SmaManager;
};

}
