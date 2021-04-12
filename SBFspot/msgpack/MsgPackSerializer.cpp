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

#include "MsgPackSerializer.h"

#include "Exporter.h"
#include "LiveData.h"

#include <msgpack.hpp>

namespace msgpack {

MsgPackSerializer::MsgPackSerializer()
{
}

MsgPackSerializer::~MsgPackSerializer()
{
}

std::vector<char> MsgPackSerializer::serialize(const LiveData& liveData) const
{
    // Pack manually (because a float in map gets stored as double and timestamp is not supported yet).
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(sbuf);
    // Map with number of elements
    uint8_t size = 5;   // Version, Timestamp, EnergyExportTotal, EnergyExportToday, Power are mandatory.
    if (!liveData.ac.empty()) ++size;
    if (!liveData.dc.empty()) ++size;
    if (liveData.energyImportTotal != 0) ++size;
    packer.pack_map(size);

    // 1. Protocol version
    packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Version));
    packer.pack_uint8(0);
    // 2. Timestamp
    packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Timestamp));
    uint32_t t = htonl(liveData.timestamp);
    packer.pack_ext(4, -1); // Timestamp type
    packer.pack_ext_body((const char*)(&t), 4);
    // 8. Consumption Total (If energy import is provided)
    if (liveData.energyImportTotal != 0) {
        packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::EnergyImportTotal));
        packer.pack_float(static_cast<float>(liveData.energyImportTotal));
    }
    // 3. Yield Total
    packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::EnergyExportTotal));
    packer.pack_float(static_cast<float>(liveData.energyExportTotal));
    // 4. Yield Today
    packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::EnergyExportToday));
    packer.pack_float(static_cast<float>(liveData.energyExportToday));
    // 5. Power AC
    packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Power));
    packer.pack(liveData.acPowerTotal);
    // 6. Data per phase
    if (!liveData.ac.empty()) {
        packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Phases));
        packer.pack_array(liveData.ac.size());
        for (uint i = 0; i < liveData.ac.size(); ++i) {
            packer.pack_map(3);
            packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Power));
            packer.pack(liveData.ac.at(i).power);
            packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Current));
            packer.pack(liveData.ac.at(i).current);
            packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Voltage));
            packer.pack(liveData.ac.at(i).voltage);
        }
    }
    // 7. Data per string array
    if (!liveData.dc.empty()) {
        packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Strings));
        packer.pack_array(liveData.dc.size());
        for (uint i = 0; i < liveData.dc.size(); ++i) {
            packer.pack_map(3);
            packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Power));
            packer.pack(liveData.dc.at(i).power);
            packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Current));
            packer.pack(liveData.dc.at(i).current);
            packer.pack_uint8(static_cast<uint8_t>(Exporter::Property::Voltage));
            packer.pack(liveData.dc.at(i).voltage);
        }
    }

    return { sbuf.data(), sbuf.data() + sbuf.size() };
}

}
