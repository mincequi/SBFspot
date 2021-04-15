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

#include "SmaInverter.h"

#include <QNetworkDatagram>
#include <QtConcurrent>

#include <Config.h>
#include <Defines.h>
#include <Ethernet_qt.h>
#include <Exporter.h>
#include <Logging.h>
#include <SBFspot.h>
#include <sma/SmaInverterRequests.h>

namespace sma {

SmaInverter::SmaInverter(QObject* parent, const Config& config, Ethernet_qt& ioDevice, Exporter& exporter, uint32_t address) :
    QObject(parent),
    m_config(config),
    m_ioDevice(ioDevice),
    m_exporter(exporter),
    m_address(address),
    m_pendingData(0)
{
    resetPendingData();

    init();
}

void SmaInverter::startPoll(std::time_t timestamp)
{
    if (m_state == State::Invalid) {
        LOG_S(WARNING) << "Inverter not yet initialized";
        return;
    }
    if (m_state == State::LoggedIn) {
        LOG_S(WARNING) << "Another poll already running";
        return;
    }

    m_pendingData.timestamp = timestamp;
    // Send login request to inverter. Once logged in, get all the data.
    login();
}

void SmaInverter::stopPoll()
{
    LOG_IF_S(WARNING, !m_pendingLris.empty()) << "Poll timed out. Discarding " << m_pendingLris.size() << " requests";

    exportData();
    logout();
}

void SmaInverter::resetPendingData()
{
    m_pendingLris.clear();
    m_pendingData = LiveData(m_serial);
    m_pendingData.ac.resize(3);
    m_pendingDataMap.clear();
}

void SmaInverter::init()
{
    // QtConcurrent::run([&](){
        auto buffer = m_sbfSpot.encodeInitRequest();
        // for (auto i = 0; (i < 5) && (m_state == State::Invalid); ++i) {
            m_ioDevice.send(buffer, m_address, 9522);
            // QThread::sleep(1);
        // }
    //});
}

void SmaInverter::login()
{
    // QtConcurrent::run([&](){
        auto buffer = m_sbfSpot.encodeLoginRequest(m_config.userGroup, std::string(m_config.SMA_Password));
        // for (auto i = 0; (i < 5) && (m_state == State::Initialized); ++i) {
            m_ioDevice.send(buffer, m_address, 9522);
            // QThread::sleep(1);
        // }
    // });
}

void SmaInverter::logout()
{
    auto buffer = m_sbfSpot.encodeLogoutRequest();
    m_ioDevice.send(buffer, m_address, 9522);
    m_state = State::Initialized;
}

void SmaInverter::requestData()
{
    requestDataSet(SoftwareVersion);
    requestDataSet(TypeLabel);
    requestDataSet(DeviceStatus);
    // Temperature is not answered by all inverter models. So, removing it.
    //requestDataSet(InverterTemperature);
    requestDataSet(MaxACPower);
    requestDataSet(EnergyProduction);
    requestDataSet(OperationTime);
    requestDataSet(SpotDCPower);
    requestDataSet(SpotDCVoltage);
    requestDataSet(SpotACPower);
    requestDataSet(SpotACVoltage);
    requestDataSet(SpotACTotalPower);
    requestDataSet(SpotGridFrequency);
}

void SmaInverter::requestDataSet(SmaInverterDataSet dataSet)
{
    auto lri = static_cast<LriDef>(SmaInverterRequests::create(dataSet).first);
    if (lri == 0) {
        LOG_S(WARNING) << "Illegal LRI";
    } else {
        m_pendingLris.insert(lri);
    }

    auto buffer = m_sbfSpot.encodeDataRequest(m_susyId, m_serial, dataSet);
    m_ioDevice.send(buffer, m_address, 9522);
}

void SmaInverter::exportData()
{
    LOG_S(3) << "Inverter" << m_serial;
    for (const auto& kv : m_pendingDataMap) {
        LOG_S(3) << "    key:" << QByteArray::number(kv.first, 16) << ", value:" << kv.second;
    }

    m_pendingData.fixup();
    m_exporter.exportLiveData(m_pendingData);
    resetPendingData();
}

void SmaInverter::onDatagram(const QNetworkDatagram& datagram)
{
    const char* data = datagram.data().data() + sizeof(ethPacketHeaderL1) - 1;
    ethPacket *pckt = (ethPacket*)data;
    switch (m_state) {
    case State::Invalid:
        m_susyId = pckt->Source.SUSyID;	// Fix Issue 98
        m_serial = pckt->Source.Serial;	// Fix Issue 98
        LOG_S(INFO) << "Inverter " << datagram.senderAddress().toString().toStdString() << ", serial: " << m_serial;
        resetPendingData();
        m_state = State::Initialized;
        return;
    case State::Initialized:
        if (btohs(pckt->ErrorCode) == 0) {
            m_state = State::LoggedIn;
            requestData();
        } else {
            LOG_S(ERROR) << "Login error. Password correct?";
        }
        return;
    case State::LoggedIn:
        m_sbfSpot.decodeResponse({ data, data + datagram.data().size() }, m_pendingDataMap, m_pendingData, m_pendingLris);
        break;
    }
}

}
