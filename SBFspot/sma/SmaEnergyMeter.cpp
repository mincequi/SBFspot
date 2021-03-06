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
#include "Logger.h"

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
    // check if it is an sma emeter packet
    SpeedwireHeader speedwirePacket(data, size);
    bool valid = speedwirePacket.checkHeader();
    if (valid) {
        uint16_t protocolID = speedwirePacket.getProtocolID();
        auto offset = speedwirePacket.getPayloadOffset();

        if (protocolID == SpeedwireHeader::sma_emeter_protocol_id) {
            //printf("RECEIVED EMETER PACKET  time 0x%016llx\n", LocalHost::getTickCountInMs());
            SpeedwireEmeterProtocol emeter(speedwirePacket);
            uint32_t serial = emeter.getSerialNumber();
            uint32_t timer = emeter.getTime();

            // extract obis data from the emeter packet and pass each obis data element to the obis filter
            int32_t signed_power_total = 0, signed_power_l1 = 0, signed_power_l2 = 0, signed_power_l3 = 0;
            LiveData liveData(serial);
            //liveData.ac.resize(3);
            void* obis = emeter.getFirstObisElement();
            while (obis != nullptr) {
                auto channel = SpeedwireEmeterProtocol::getObisChannel(obis);
                auto index = SpeedwireEmeterProtocol::getObisIndex(obis);
                auto type = SpeedwireEmeterProtocol::getObisType(obis);
                bool doLog = ((index%10) == 1) || ((index%10) == 2);
                uint64_t value = (type == 4) ? SpeedwireEmeterProtocol::getObisValue4(obis) : SpeedwireEmeterProtocol::getObisValue8(obis);
                LOG_IF_F(2, doLog, "OBIS %i:%i.%i.0: %llu", channel, index, type, value);

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
                    case 31: liveData.ac.at(0).current = value/1000.0f; break;
                    case 32: liveData.ac.at(0).voltage = value/10.0f; break;
                    case 51: liveData.ac.at(1).current = value/1000.0f; break;
                    case 52: liveData.ac.at(1).voltage = value/10.0f; break;
                    case 71: liveData.ac.at(2).current = value/1000.0f; break;
                    case 72: liveData.ac.at(2).voltage = value/10.0f; break;
                    }
                } else if (SpeedwireEmeterProtocol::getObisType(obis) == 8) {
                    uint64_t value = SpeedwireEmeterProtocol::getObisValue8(obis);
                    switch (SpeedwireEmeterProtocol::getObisIndex(obis)) {
                    case  1: liveData.energyImportTotal = value/3600;  break;
                    case  2: liveData.energyExportTotal = value/3600;  break;
                    }
                }
                obis = emeter.getNextObisElement(obis);
            }
            // send the calculated signed power values to the obis filter
            auto obis_signed_power_total = ObisData::SignedActivePowerTotal.toByteArray();
            auto obis_signed_power_L1    = ObisData::SignedActivePowerL1.toByteArray();
            auto obis_signed_power_L2    = ObisData::SignedActivePowerL2.toByteArray();
            auto obis_signed_power_L3    = ObisData::SignedActivePowerL3.toByteArray();
            SpeedwireByteEncoding::setUint32BigEndian(&obis_signed_power_total[4], signed_power_total);
            SpeedwireByteEncoding::setUint32BigEndian(&obis_signed_power_L1   [4], signed_power_l1);
            SpeedwireByteEncoding::setUint32BigEndian(&obis_signed_power_L2   [4], signed_power_l2);
            SpeedwireByteEncoding::setUint32BigEndian(&obis_signed_power_L3   [4], signed_power_l3);
            liveData.acPowerTotal = signed_power_total/10;
            liveData.ac.at(0).power = signed_power_l1/10;
            liveData.ac.at(1).power = signed_power_l2/10;
            liveData.ac.at(2).power = signed_power_l3/10;
            liveData.timestamp = time(nullptr);
            return liveData;
        }
    }

    return LiveData(0);
}

}
