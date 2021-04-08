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

#include "Config.h"
#include "Exporter.h"
#include "LiveData.h"

namespace sma {

SmaManager::SmaManager(Config& config, Exporter& exporter)
    : m_config(config),
      m_exporter(exporter),
      m_ethernet(*this),
      m_discoverTimer(startTimer(1*60*1000)),   // discover every 60 seconds
      m_timeComputation(config)
{
    srand(time(nullptr));
    AppSerial = 900000000 + ((rand() << 16) + rand()) % 100000000;

    connect(&m_liveTimer, &QTimer::timeout, this, &SmaManager::onLiveTimeout);
    m_liveTimer.setSingleShot(true);

    startNextLiveTimer();
}

void SmaManager::discoverDevices()
{
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
    m_exporter.exportLiveData(liveData);
}

void SmaManager::onDiscoveryResponseDatagram(const QNetworkDatagram& datagram)
{
    auto ip = datagram.senderAddress().toIPv4Address();
    if (!m_inverters.count(ip)) {
        qInfo() << "Found inverter at: " << datagram.senderAddress();
        m_inverters.emplace(ip, new SmaInverter(this, m_config, m_ethernet, m_exporter, ip));
        m_inverters.at(ip)->m_lastSeen = std::time(nullptr);
    } else {
        m_inverters.at(ip)->m_lastSeen = std::time(nullptr);
    }
}

void SmaManager::onUnknownDatagram(const QNetworkDatagram& datagram)
{
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
    qInfo() << "Live timeout:" << m_currentTimePoint;
    for (auto& kv : m_inverters) {
        if (kv.second->m_state == SmaInverter::State::Invalid)
            continue;

        kv.second->poll(m_currentTimePoint);
    }

    startNextLiveTimer();
}

void SmaManager::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == m_discoverTimer) {
        auto now = std::time_t(nullptr);
        auto it = m_inverters.begin();
        while (it != m_inverters.end()) {
            if (now - it->second->m_lastSeen > 3600) {
                qInfo() << "Inverter at ip" << QHostAddress(it->first) << "not seen for an hour. Removing.";
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