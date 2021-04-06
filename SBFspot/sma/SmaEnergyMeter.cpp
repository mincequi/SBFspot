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

#include "SmaEnergyMeter.h"

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

namespace sma {

SmaEnergyMeter::SmaEnergyMeter() {
}

SmaEnergyMeter::~SmaEnergyMeter() {
}

LiveData SmaEnergyMeter::parsePacket(const char* data, uint16_t size)
{
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
    filter.addFilter(ObisData::CurrentL1);
    filter.addFilter(ObisData::CurrentL2);
    filter.addFilter(ObisData::CurrentL3);
    filter.addFilter(ObisData::VoltageL1);
    filter.addFilter(ObisData::VoltageL2);
    filter.addFilter(ObisData::VoltageL3);
    filter.addFilter(ObisData::SignedActivePowerTotal);   // calculated value that is not provided by emeter
    filter.addFilter(ObisData::SignedActivePowerL1);      // calculated value that is not provided by emeter
    filter.addFilter(ObisData::SignedActivePowerL2);      // calculated value that is not provided by emeter
    filter.addFilter(ObisData::SignedActivePowerL3);      // calculated value that is not provided by emeter

    // check if it is an sma emeter packet
    SpeedwireHeader protocol(data, size);
    bool valid = protocol.checkHeader();
    if (valid) {
        uint32_t group = protocol.getGroup();
        uint16_t length = protocol.getLength();
        uint16_t protocolID = protocol.getProtocolID();
        int      offset = protocol.getPayloadOffset();

        if (protocolID == SpeedwireHeader::sma_emeter_protocol_id) {
            //printf("RECEIVED EMETER PACKET  time 0x%016llx\n", LocalHost::getTickCountInMs());
            SpeedwireEmeterProtocol emeter(data + offset, size - offset);
            uint16_t susyid = emeter.getSusyID();
            uint32_t serial = emeter.getSerialNumber();
            uint32_t timer = emeter.getTime();

            // extract obis data from the emeter packet and pass each obis data element to the obis filter
            int32_t signed_power_total = 0, signed_power_l1 = 0, signed_power_l2 = 0, signed_power_l3 = 0;
            LiveData liveData;
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
                    case 31: liveData.acCurrent.at(0) = value/1000.0f; break;
                    case 32: liveData.acVoltage.at(0) = value/10.0f; break;
                    case 51: liveData.acCurrent.at(1) = value/1000.0f; break;
                    case 52: liveData.acVoltage.at(1) = value/10.0f; break;
                    case 71: liveData.acCurrent.at(2) = value/1000.0f; break;
                    case 72: liveData.acVoltage.at(2) = value/10.0f; break;
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
            liveData.isValid = true;
            liveData.deviceType = ElectricityMeter;
            liveData.serial = serial;
            liveData.totalPower = signed_power_total/10;
            liveData.acPower.at(0) = signed_power_l1/10;
            liveData.acPower.at(1) = signed_power_l2/10;
            liveData.acPower.at(2) = signed_power_l3/10;
            liveData.timestamp = time(nullptr);
            return liveData;
        }
    }

    return {};
}

}