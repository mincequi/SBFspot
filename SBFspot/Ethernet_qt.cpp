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

#include "Ethernet_qt.h"

#include <QNetworkDatagram>
#include <QNetworkInterface>

// TODO: remove SMA specific types out of here
#include <Logger.h>
#include <sma/SmaManager.h>

Ethernet_qt::Ethernet_qt(sma::SmaManager& processor)
    : m_processor(processor)
{
    m_udpSocket.bind(QHostAddress::AnyIPv4, 9522, QUdpSocket::ShareAddress);
    m_udpSocket.joinMulticastGroup(QHostAddress("239.12.255.254"));

    QObject::connect(&m_udpSocket, &QUdpSocket::readyRead, this, &Ethernet_qt::onReadyRead);
}

void Ethernet_qt::send(const ByteBuffer& data, uint32_t address, uint16_t port)
{
    // TODO: implement stream operator for std::vector<uint8_t>
    LOG_S(2) << "Send datagram: " << QByteArray(reinterpret_cast<const char*>(data.data()), data.size());
    m_udpSocket.writeDatagram(reinterpret_cast<const char*>(data.data()), data.size(), QHostAddress(address), port);
}

void Ethernet_qt::send(const QByteArray& datagram, const std::string& address, uint16_t port)
{
    m_udpSocket.writeDatagram(datagram, QHostAddress(QString::fromStdString(address)), port);
}

void Ethernet_qt::onReadyRead()
{
    auto addresses = QNetworkInterface::allAddresses();

    while (m_udpSocket.hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket.receiveDatagram();
        if (addresses.contains(datagram.senderAddress())) {
            LOG_F(2, "Discard datagram from localhost");
        } else if (datagram.data().size() == 600 || datagram.data().size() == 608) {
            //LOG_F(1, "Received energy meter datagram. size: %i", datagram.data().size());
            m_processor.onEnergyMeterDatagram(datagram);
        } else if (datagram.data().startsWith(QByteArray::fromHex("534d4100000402A000000001000200000001"))) {
            LOG_F(2, "Received discovery response datagram. size: %i", datagram.data().size());
            m_processor.onDiscoveryResponseDatagram(datagram);
        } else if (datagram.senderAddress() == QHostAddress::LocalHost) {
            LOG_F(2, "Discard datagram from localhost");
        } else {
            LOG_F(2, "Received datagram from: %s, size: %i bytes", datagram.senderAddress().toString().toStdString().c_str(), datagram.data().size());
            auto buffer = datagram.data();
            ethPacket *pckt = (ethPacket *)(datagram.data().data() + sizeof(ethPacketHeaderL1) - 1);
            m_processor.onUnknownDatagram(datagram);
        }
    }
}