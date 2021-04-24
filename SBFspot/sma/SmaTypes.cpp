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

#include "SmaTypes.h"

#include <iomanip>
#include <iostream>

#include <SpeedwireHeader.hpp>
#include <SpeedwireInverterProtocol.hpp>

#include <Logger.h>

SmaPacket SmaPacket::fromBuffer(ByteBuffer& buffer) {
    buffer.reset();
    SmaPacket packet;
    packet.smaSignatureL1 = buffer.readUint32();
    packet.smaTag = buffer.readUint32();
    packet.smaGroup = buffer.readUint32();
    packet.size = buffer.readUint16();

    packet.version = buffer.readUint16();
    packet.type = static_cast<SmaPacketType>(buffer.readUint16());
    packet.longCount = buffer.readUint8();
    packet.control = buffer.readUint8();

    packet.destinationSusyId = buffer.readUint16(false);
    packet.destinationSerial = buffer.readUint32(false);;
    packet.destinationControl = buffer.readUint16(false);

    packet.sourceSusyId = buffer.readUint16(false);
    packet.sourceSerial = buffer.readUint32(false);
    packet.sourceControl = buffer.readUint16(false);

    packet.error = buffer.readUint16(false);
    packet.fragmentId = buffer.readUint16(false);
    packet.packetId = buffer.readUint16(false);

    packet.dataSet = static_cast<SmaInverterDataSet>(buffer.readUint32(false));
    packet.first = buffer.readUint32();
    packet.last = buffer.readUint32();

    packet.payload = buffer.readByteArray(std::numeric_limits<uint16_t>::max());

    return packet;
}

std::ostream& operator<< (std::ostream& out, SmaPacket const& c) {
    //out << "signature: " << std::uppercase << std::hex << c.smaSignatureL1;
    out << std::endl;
    out << "tag: " << std::uppercase << std::hex << c.smaTag;
    out << ", net: " << std::dec << c.smaGroup;
    out << ", size: " << c.size;
    out << ", network version: " << c.version;
    out << ", type: " << std::hex << static_cast<uint16_t>(c.type);
    //out << ", long words: " << std::dec << static_cast<uint32_t>(c.longCount);
    out << ", control: " << std::dec << static_cast<uint32_t>(c.control);

    out << "," << std::endl;
    out << "to: { ";
    out << "susyId: " << static_cast<uint16_t>(c.destinationSusyId);
    out << ", serial: " << static_cast<uint32_t>(c.destinationSerial);
    out << ", control: " << static_cast<uint16_t>(c.destinationControl) << " }," << std::endl;
    out << "from: { ";
    out << "susyId: " << static_cast<uint16_t>(c.sourceSusyId);
    out << ", serial: " << static_cast<uint32_t>(c.sourceSerial);
    out << ", control: " << static_cast<uint16_t>(c.sourceControl) << "}," << std::endl;

    out << "error: " << c.error;
    out << ", fragment: " << c.fragmentId;
    out << ", packet: " << c.packetId << std::endl;

    out << "dataset: " << c.dataSet;
    out << ", first: " << c.first;
    out << ", last: " << c.last << std::endl;
    return out;
}

std::ostream& operator<< (std::ostream& out, SpeedwireHeader const& c) {
    if (!c.checkHeader()) {
        return out << "Speedwire header is invalid!";
    }

    out << "signature: " << std::uppercase << std::hex << c.getSignature();
    out << ", tag0: " << c.getTag0();
    out << ", group: " << std::dec << c.getGroup();
    out << ", length: " << c.getLength();
    out << ", network version: " << std::hex << c.getNetworkVersion();
    out << ", protocol id: " << c.getProtocolID();
    out << ", long words: " << static_cast<uint32_t>(c.getGroup());
    out << ", control: " << static_cast<uint32_t>(c.getControl());

    return out;
}

std::ostream& operator<< (std::ostream& out, SpeedwireInverterProtocol const& c) {

    out << "DstSusyID: " << c.getDstSusyID();
    out << ", DstSerialNumber: " << c.getDstSerialNumber();
    out << ", DstControl: " << c.getDstControl();
    out << ", SrcSusyID: " << c.getSrcSusyID();
    out << ", SrcSerialNumber: " << c.getSrcSerialNumber();
    out << ", SrcControl: " << c.getSrcControl();
    out << ", ErrorCode: " << c.getErrorCode();
    out << ", FragmentID: " << c.getFragmentID();
    out << ", PacketID: " << c.getPacketID();
    out << ", CommandID: " << std::hex << c.getCommandID();
    out << ", FirstRegisterID: " << c.getFirstRegisterID();
    out << ", LastRegisterID: " << c.getLastRegisterID();

    return out;
}

