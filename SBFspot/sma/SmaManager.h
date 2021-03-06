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

#include <QTimer>

#include <Ethernet_qt.h>
#include <Timer.h>
#include <sma/SmaInverter.h>
#include <sma/SmaEnergyMeter.h>
#include <sma/SmaRequestStrategy.h>
#include <msgpack/MsgPackSerializer.h>

class QByteArray;

class Config;
class Exporter;

namespace sma {

class SmaManager : public QObject {
    Q_OBJECT

public:
    SmaManager(Config& config, Exporter& exporter, Storage* storage);

    void discoverDevices();

    const std::map<uint32_t, SmaInverter*>& inverters() const;

private:
    void onEnergyMeterDatagram(const QNetworkDatagram& datagram);
    void onDiscoveryResponseDatagram(const QNetworkDatagram& datagram);
    void onUnknownDatagram(const QNetworkDatagram& datagram);

    void startNextLiveTimer();
    void onLiveTimeout();
    void onPollTimeout();
    void timerEvent(QTimerEvent *event) override;

    const Config&   m_config;
    Exporter&       m_exporter;
    Storage*        m_storage = nullptr;

    Ethernet_qt m_ethernet;
    sma::SmaEnergyMeter     m_energyMeter;

    int m_discoverTimer = 0;
    std::map<uint32_t, SmaInverter*> m_inverters;
    SmaRequestStrategy  m_requestStrategy;

    Timer  m_timeComputation;
    QTimer m_liveTimer;
    QTimer m_archiveTimer;
    QTimer  m_pollTimer;
    std::time_t m_currentTimePoint = 0;

    friend class ::Ethernet_qt;
};

} // namespace sma
