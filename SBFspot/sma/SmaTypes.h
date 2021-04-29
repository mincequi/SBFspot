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

#include <variant>

#include <LiveData.h>
#include <Types.h>

class SpeedwireHeader;
class SpeedwireInverterProtocol;

using SmaResponse = std::variant<LiveData, std::vector<DayData>, std::vector<MonthData>>;

enum class SmaPacketType : uint16_t {
    Invalid = 0x0000,
    Inverter = 0x6065,
    EnergyMeter = 0x6069,
    Discovery = 0xFFFF
};

struct SmaPacket {
    static SmaPacket fromBuffer(ByteBuffer& buffer);

    uint32_t smaSignatureL1 = 0;    // 53 4D 41 00
    uint32_t smaTag = 0;            // 00 04 02 A0
    uint32_t smaGroup = 0;          // 00 00 00 01
    uint16_t size = 0;              // size in bytes
//14
    uint16_t version = 0;           // 00 10
    SmaPacketType type = SmaPacketType::Invalid;    // Inverter: 0x6065, Energy meter: 0x6069, Discovery: 0xFFFF
    uint8_t  longCount = 0;         // count of uint32s (size/4)
    uint8_t  control= 0;            // ?
//20
    uint16_t destinationSusyId;
    uint32_t destinationSerial;
    uint16_t destinationControl;    // ?
//28
    uint16_t sourceSusyId;
    uint32_t sourceSerial;
    uint16_t sourceControl;     // ?
//36
    uint16_t error;
    uint16_t fragmentId;  //Count Down
    uint16_t packetId;    //Count Up
//42
    SmaInverterDataSet dataSet = Invalid;
    uint32_t first;
    uint32_t last;
//54
    ByteBuffer payload; // 54?
};
std::ostream& operator<< (std::ostream& out, SmaPacket const& c);

// Nameplate response
// 0  // 53 4D 41 00 00 04 02 A0  00 00 00 01 00 4E 00 10 // signatureL1 + tag   group + size + version
// 16 // 60 65 13 A0 7D 00 D9 96  77 37 00 A0 98 01 CC 24 // type(0x6065) + longCount(0x13) + control(0xA0) + susyid(0x9801) + serial(0xCC24)
// 32 // 3A B3 00 00 00 00 00 00  04 80 01 02 00 58 0A 00 // sourceSerial(0x3AB3)
// 48 // 00 00 0A 00 00 00 01 34  82 00 7E F9 7A 60 00 00 // ... lri (0x1), dataset(0x3482)
// 64 // 00 00 00 00 00 00 FE FF  FF FF FE FF FF FF 04 0F

// DayData response
// 0  // 53 4D 41 00 00 04 02 A0  00 00 00 01 03 F2 00 10
// 16 // 60 65 FC E0 7D 00 D9 96  77 37 00 A0 98 01 CC 24 // protocol(0x6065) + longCount(0xFC), control(0xE0)
// 32 // 3A B3 00 00 00 00 05 00  05 80 01 02 00 70 77 31 // ... fragment (0x0500)
// 48 // 00 00 C7 31 00 00 4C 6E  7B 60 F7 99 0D 00 00 00 // timestamp(0x4C6E7B60) + value(0xF7990D00 00000000)
// 64 // 00 00 78 6F 7B 60 F7 99  0D 00 00 00 00 00 A4 70 // ... timestamp(0x786F7B60) + value(0xF7990D00 00000000)

std::ostream& operator<< (std::ostream& out, SpeedwireHeader const& c);
std::ostream& operator<< (std::ostream& out, SpeedwireInverterProtocol const& c);