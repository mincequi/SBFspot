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

#include <QDebug>
#include <QNetworkDatagram>
#include <QtConcurrent>

#include <Config.h>
#include <Ethernet_qt.h>
#include <SBFspot.h>
#include <sma/SmaInverterRequests.h>

namespace sma {

SmaInverter::SmaInverter(const Config& config, Ethernet_qt& ioDevice, uint32_t address) :
    m_config(config),
    m_ioDevice(ioDevice),
    m_address(address)
{
    init();
}

void SmaInverter::poll()
{
    if (m_state == State::Invalid) {
        qWarning() << "Inverter not yet initialized";
        return;
    }
    if (m_state == State::LoggedIn) {
        qWarning() << "Another poll already running";
        return;
    }

    // Send login request to inverter. Once logged in, get all the data.
    login();
}

void SmaInverter::init()
{
    QtConcurrent::run([&](){
        std::vector<uint8_t> buffer(256);
        SbfSpot::encodeInitRequest(buffer.data());
        buffer.resize(packetposition);
        for (auto i = 0; (i < 5) && (m_state == State::Invalid); ++i) {
            m_ioDevice.send(buffer, m_address, 9522);
            QThread::sleep(1);
        }
    });
}

void SmaInverter::login()
{
    QtConcurrent::run([&](){
        std::vector<uint8_t> buffer;
        SbfSpot::encodeLoginRequest(buffer, m_config.userGroup, std::string(m_config.SMA_Password));
        for (auto i = 0; (i < 5) && (m_state == State::Initialized); ++i) {
            m_ioDevice.send(buffer, m_address, 9522);
            QThread::sleep(1);
        }
    });
}

void SmaInverter::logout()
{
    std::vector<uint8_t> buffer(256);
    SbfSpot::encodeLogoutRequest(buffer.data());
    buffer.resize(packetposition);
    m_ioDevice.send(buffer, m_address, 9522);
    m_state = State::Initialized;
}

void SmaInverter::requestData()
{
    requestDataSet(SoftwareVersion);
    requestDataSet(TypeLabel);
    requestDataSet(DeviceStatus);
    requestDataSet(InverterTemperature);
    requestDataSet(MaxACPower);
    requestDataSet(EnergyProduction);
    requestDataSet(OperationTime);
    requestDataSet(SpotDCPower);
    requestDataSet(SpotDCVoltage);
    requestDataSet(SpotACPower);
    requestDataSet(SpotACVoltage);
    requestDataSet(SpotACTotalPower);
    requestDataSet(SpotGridFrequency);

    QtConcurrent::run([&](){
        QThread::sleep(1);
        // TODO: no retry behaviour for now
        // If some data is still pending, retry
        // for (auto i = 0; (i < 5) && !m_pendingLris.empty(); ++i) {
        //     for (const auto lri : m_pendingLris) {
        //         requestDataSet(SmaInverterRequests::create(lri).dataSet);
        //     }
        //     QThread::sleep(1);
        // }
        exportData();
        logout();
    });
}

void SmaInverter::requestDataSet(SmaInverterDataSet dataSet)
{
    auto lri = static_cast<LriDef>(SmaInverterRequests::create(dataSet).first);
    if (lri == 0) {
        qWarning() << "Illegal LRI";
    } else {
        m_pendingLris.insert(lri);
    }

    std::vector<uint8_t> buffer(1024);
    SbfSpot::encodeDataRequest(buffer.data(), m_susyId, m_serial, dataSet);
    buffer.resize(packetposition);
    m_ioDevice.send(buffer, m_address, 9522);
}

void SmaInverter::exportData()
{
    qInfo() << "Inverter" << m_serial;
    for (const auto& kv : m_pendingDataMap) {
        qInfo() << "    key:" << QByteArray::number(kv.first, 16) << ", value:" << kv.second;
    }

    m_pendingLris.clear();
    m_pendingData = InverterData();
    m_pendingDataMap.clear();
}

void SmaInverter::onDatagram(const QNetworkDatagram& datagram)
{
    qDebug() << "Received datagram from:" << datagram.senderAddress();

    const char* data = datagram.data().data() + sizeof(ethPacketHeaderL1) - 1;
    ethPacket *pckt = (ethPacket*)data;
    switch (m_state) {
    case State::Invalid:
        m_susyId = pckt->Source.SUSyID;	// Fix Issue 98
        m_serial = pckt->Source.Serial;	// Fix Issue 98
        m_state = State::Initialized;
        return;
    case State::Initialized:
        if (btohs(pckt->ErrorCode) == 0) {
            m_state = State::LoggedIn;
            requestData();
        } else {
            qWarning() << "Login error. Password correct?";
        }
        return;
    case State::LoggedIn:
        SbfSpot::decodeResponse(reinterpret_cast<const uint8_t*>(data), m_pendingDataMap, m_pendingData, m_pendingLris);
        qDebug() << "Pending LRIs:" << m_pendingLris.size();
        break;
    }
}

}
