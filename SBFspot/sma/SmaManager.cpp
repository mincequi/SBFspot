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

#include "SmaManager.h"

#include <QByteArray>
#include <QNetworkDatagram>
#include <QTimer>
#include <QtConcurrent>

#include <SpeedwireHeader.hpp>
#include <SpeedwireInverterProtocol.hpp>

#include "Config.h"
#include "Exporter.h"
#include "LiveData.h"
#include "Logger.h"
#include "misc.h"

namespace sma {

SmaManager::SmaManager(Config& config, Exporter& exporter, Storage* storage) :
    m_config(config),
    m_exporter(exporter),
    m_storage(storage),
    m_ethernet(*this),
    m_discoverTimer(startTimer(1*60*1000)),   // discover every 60 seconds
    m_requestStrategy(config),
    m_timeComputation(config)
{   
    srand(time(nullptr));
    AppSerial = 900000000 + ((rand() << 16) + rand()) % 100000000;

    connect(&m_liveTimer, &QTimer::timeout, this, &SmaManager::onLiveTimeout);
    m_liveTimer.setSingleShot(true);

    connect(&m_pollTimer, &QTimer::timeout, this, &SmaManager::onPollTimeout);
    m_pollTimer.setSingleShot(true);
    m_pollTimer.setInterval(1000);

    if (m_config.command == Config::Command::RunDaemon) {
        startNextLiveTimer();
    }
}

void SmaManager::discoverDevices()
{
    LOG_S(INFO) << "Discovering SMA devices";
    QtConcurrent::run([this](){
        for (auto i = 0; i < 5 ;++i) {
            m_ethernet.send(QByteArray::fromHex("534D4100000402A0FFFFFFFF0000002000000000"), "239.12.255.254", 9522);
            QThread::sleep(1);
        }
    });
}

const std::map<uint32_t, SmaInverter*>& SmaManager::inverters() const
{
    return m_inverters;
}

void SmaManager::onEnergyMeterDatagram(const QNetworkDatagram& buffer)
{
    auto liveData = m_energyMeter.parsePacket(buffer.data().data(), buffer.data().size());
    if (liveData.serial != 0) {
        m_exporter.open();
        m_exporter.exportLiveData(liveData);
        m_exporter.close();
    }
}

void SmaManager::onDiscoveryResponseDatagram(const QNetworkDatagram& datagram)
{
    auto ip = datagram.senderAddress().toIPv4Address();
    if (!m_inverters.count(ip)) {
        LOG_S(INFO) << "Discovered inverter at: " << datagram.senderAddress().toString().toStdString();
        auto inverter = new SmaInverter(this, m_config, m_ethernet, ip, m_storage);
        inverter->m_lastSeen = std::time(nullptr);
        m_inverters.emplace(ip, inverter);
        m_requestStrategy.addInverter(inverter);
    } else {
        m_inverters.at(ip)->m_lastSeen = std::time(nullptr);
    }
}

void SmaManager::onUnknownDatagram(const QNetworkDatagram& datagram)
{
    auto ba = datagram.data();
    ByteBuffer buffer(ba.begin(), ba.end());
    auto packet = SmaPacket::fromBuffer(buffer);
    LOG_S(2) << packet;
    //LOG_S(INFO) << buffer;

    //const ethPacket* pckt = (ethPacket*)datagram.data().data();

    auto ip = datagram.senderAddress().toIPv4Address();
    if (m_inverters.count(ip)) {
        m_inverters.at(ip)->onDatagram(datagram);
    }
}

void SmaManager::startNextLiveTimer()
{
    m_currentTimePoint = m_timeComputation.nextTimePoint();
    auto now = std::time(nullptr);
    auto ms = (m_currentTimePoint - now)*1000;
    m_liveTimer.start(ms);
    //m_liveTimer.start(10000);
}

void SmaManager::onLiveTimeout()
{
    bool pollStarted = false;

    LOG_S(INFO) << "Polling inverters, timestamp: " << m_currentTimePoint;
    for (auto& kv : m_inverters) {
        if (kv.second->m_state == SmaInverter::State::Invalid) {
            kv.second->init();
            continue;
        }

        pollStarted = true;
        kv.second->login(m_currentTimePoint);
    }

    if (pollStarted) m_pollTimer.start();

    startNextLiveTimer();
}

void SmaManager::onPollTimeout()
{
    LOG_IF_F(ERROR, !m_exporter.open(), "Error opening database");

    LOG_S(1) << "Polling inverters finished";
    for (auto& kv : m_inverters) {
        if (kv.second->m_state != SmaInverter::State::LoggedIn)
            continue;

        auto results = kv.second->result();
        for (const auto& result : results) {
            std::visit([this](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, LiveData>)
                    m_exporter.exportLiveData(arg);
                else if constexpr (std::is_same_v<T, std::vector<DayData>>) {
                    m_exporter.exportDayData(arg);
                }
                else if constexpr (std::is_same_v<T, std::vector<MonthData>>)
                    m_exporter.exportMonthData(arg);
            }, result);
        }
    }

    m_exporter.close();
}

void SmaManager::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_discoverTimer) {
        auto now = std::time_t(nullptr);
        auto it = m_inverters.begin();
        while (it != m_inverters.end()) {
            if (now - it->second->m_lastSeen > 3600) {
                LOG_S(INFO) << "Inverter with ip: " << asIp(it->first) << " not seen for an hour. Removing.";
                delete it->second;
                it = m_inverters.erase(it);
            } else {
                ++it;
            }
        }
        discoverDevices();
    }
}

} // namespace sma
