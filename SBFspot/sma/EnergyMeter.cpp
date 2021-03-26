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

#include "EnergyMeter.h"

#include "LiveData.h"

#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2tcpip.h>
#define poll(a, b, c)  WSAPoll((a), (b), (c))
#else
#include <poll.h>
#endif

#include <SpeedwireSocketFactory.hpp>
#include <SpeedwireByteEncoding.hpp>
#include <SpeedwireHeader.hpp>
#include <SpeedwireEmeterProtocol.hpp>
#include <ObisFilter.hpp>

/**
 +  poll all configured sockets for emeter udp packets and pass emeter data to the obis filter
 */
static bool poll_emeters(const std::vector<SpeedwireSocket>& sockets,
                         struct pollfd* const fds,
                         const int timeout,
                         ObisFilter& filter,
                         LiveData* liveData) {
    unsigned char multicast_packet[1024];

    // prepare the pollfd structure
    for (int j = 0; j < sockets.size(); ++j) {
        fds[j].fd      = sockets[j].getSocketFd();
        fds[j].events  = POLLIN;
        fds[j].revents = 0;
    }

    // wait for a packet on the configured socket
    if (poll(fds, sockets.size(), timeout) < 0) {
        perror("poll failure");
        return false;
    }

    // determine if the socket received a packet
    for (int j = 0; j < sockets.size(); ++j) {
        auto& socket = sockets[j];

        if ((fds[j].revents & POLLIN) != 0) {
            int nbytes = -1;

            // read packet data
            if (socket.isIpv4()) {
                struct sockaddr_in src;
                nbytes = socket.recvfrom(multicast_packet, sizeof(multicast_packet), src);
            }
            else if (socket.isIpv6()) {
                struct sockaddr_in6 src;
                nbytes = socket.recvfrom(multicast_packet, sizeof(multicast_packet), src);
            }
            // check if it is an sma emeter packet
            SpeedwireHeader protocol(multicast_packet, nbytes);
            bool valid = protocol.checkHeader();
            if (valid) {
                uint32_t group = protocol.getGroup();
                uint16_t length = protocol.getLength();
                uint16_t protocolID = protocol.getProtocolID();
                int      offset = protocol.getPayloadOffset();

                if (protocolID == SpeedwireHeader::sma_emeter_protocol_id) {
                    printf("RECEIVED EMETER PACKET  time 0x%016llx\n", LocalHost::getTickCountInMs());
                    SpeedwireEmeterProtocol emeter(multicast_packet + offset, nbytes - offset);
                    uint16_t susyid = emeter.getSusyID();
                    uint32_t serial = emeter.getSerialNumber();
                    uint32_t timer = emeter.getTime();

                    // extract obis data from the emeter packet and pass each obis data element to the obis filter
                    int32_t signed_power_total = 0, signed_power_l1 = 0, signed_power_l2 = 0, signed_power_l3 = 0;
                    void* obis = emeter.getFirstObisElement();
                    while (obis != NULL) {
                        //emeter.printObisElement(obis, stderr);
                        // ugly hack to calculate the signed power value
                        if (SpeedwireEmeterProtocol::getObisType(obis) == 4) {
                            uint32_t value = SpeedwireEmeterProtocol::getObisValue4(obis);
                            switch (SpeedwireEmeterProtocol::getObisIndex(obis)) {
                            case  1: signed_power_total += value;  break;
                            case  2: signed_power_total -= value;  break;
                            case 21: signed_power_l1    += value;  break;
                            case 22: signed_power_l1    -= value;  break;
                            case 41: signed_power_l2    += value;  break;
                            case 42: signed_power_l2    -= value;  break;
                            case 61: signed_power_l3    += value;  break;
                            case 62: signed_power_l3    -= value;  break;
                            }
                        }
                        // send the obis value to the obis filter before proceeding with then next obis element
                        filter.consume(obis, timer);
                        obis = emeter.getNextObisElement(obis);
                    }
                    // send the calculated signed power values to the obis filter
                    std::array<uint8_t, 8> obis_signed_power_total = ObisData::SignedActivePowerTotal.toByteArray();
                    std::array<uint8_t, 8> obis_signed_power_L1    = ObisData::SignedActivePowerL1.toByteArray();
                    std::array<uint8_t, 8> obis_signed_power_L2    = ObisData::SignedActivePowerL2.toByteArray();
                    std::array<uint8_t, 8> obis_signed_power_L3    = ObisData::SignedActivePowerL3.toByteArray();
                    SpeedwireByteEncoding::setUint32BigEndian(&obis_signed_power_total[4], signed_power_total);
                    SpeedwireByteEncoding::setUint32BigEndian(&obis_signed_power_L1   [4], signed_power_l1);
                    SpeedwireByteEncoding::setUint32BigEndian(&obis_signed_power_L2   [4], signed_power_l2);
                    SpeedwireByteEncoding::setUint32BigEndian(&obis_signed_power_L3   [4], signed_power_l3);
                    filter.consume(obis_signed_power_total.data(), timer);
                    filter.consume(obis_signed_power_L1.data(), timer);
                    filter.consume(obis_signed_power_L2.data(), timer);
                    filter.consume(obis_signed_power_L3.data(), timer);
                    if (liveData) {
                        liveData->isValid = true;
                        liveData->deviceType = ElectricityMeter;
                        liveData->serial = serial;
                        liveData->powerTotal = signed_power_total/10;
                    }
                    return true;
                }
            }
        }
    }

    return false;
}

namespace sma {

EnergyMeter::EnergyMeter() {
}

EnergyMeter::~EnergyMeter() {
}

LiveData EnergyMeter::importLiveData() const {
    // define measurement filters for sma emeter packet filtering
    ObisFilter filter;
    //filter.addFilter(ObisData::PositiveActivePowerTotal);
    //filter.addFilter(ObisData::PositiveActivePowerL1);
    //filter.addFilter(ObisData::PositiveActivePowerL2);
    //filter.addFilter(ObisData::PositiveActivePowerL3);
    //filter.addFilter(ObisData::PositiveActiveEnergyTotal);
    //filter.addFilter(ObisData::PositiveActiveEnergyL1);
    //filter.addFilter(ObisData::PositiveActiveEnergyL2);
    //filter.addFilter(ObisData::PositiveActiveEnergyL3);
    //filter.addFilter(ObisData::NegativeActivePowerTotal);
    //filter.addFilter(ObisData::NegativeActivePowerL1);
    //filter.addFilter(ObisData::NegativeActivePowerL2);
    //filter.addFilter(ObisData::NegativeActivePowerL3);
    //filter.addFilter(ObisData::NegativeActiveEnergyTotal);
    //filter.addFilter(ObisData::NegativeActiveEnergyL1);
    //filter.addFilter(ObisData::NegativeActiveEnergyL2);
    //filter.addFilter(ObisData::NegativeActiveEnergyL3);
    //filter.addFilter(ObisData::PowerFactorTotal);
    //filter.addFilter(ObisData::PowerFactorL1);
    //filter.addFilter(ObisData::PowerFactorL2);
    //filter.addFilter(ObisData::PowerFactorL3);
    //filter.addFilter(ObisData::CurrentL1);
    //filter.addFilter(ObisData::CurrentL2);
    //filter.addFilter(ObisData::CurrentL3);
    //filter.addFilter(ObisData::VoltageL1);
    //filter.addFilter(ObisData::VoltageL2);
    //filter.addFilter(ObisData::VoltageL3);
    filter.addFilter(ObisData::SignedActivePowerTotal);   // calculated value that is not provided by emeter
    filter.addFilter(ObisData::SignedActivePowerL1);      // calculated value that is not provided by emeter
    filter.addFilter(ObisData::SignedActivePowerL2);      // calculated value that is not provided by emeter
    filter.addFilter(ObisData::SignedActivePowerL3);      // calculated value that is not provided by emeter

    // open socket(s) to receive sma emeter packets from any local interface
    LocalHost localhost;
    const std::vector<SpeedwireSocket> sockets = SpeedwireSocketFactory::getInstance(localhost)->getRecvSockets(SpeedwireSocketFactory::MULTICAST, localhost.getLocalIPv4Addresses());

    // allocate pollfd struct(s) for the socket(s)
    struct pollfd *const fds = (struct pollfd *const) malloc(sizeof(struct pollfd) * sockets.size());

    // poll sockets for inbound emeter packets
    LiveData liveData;
    if (!poll_emeters(sockets, fds, 1000, filter, &liveData)) {
        perror("Error polling emeter");
    }

    free(fds);

    return liveData;
}

}