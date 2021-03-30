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

#include "Export.h"
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
    if (!liveData.isValid) {
        return {};
    }

    // Pack manually (because a float in map gets stored as double and timestamp is not supported yet).
    msgpack::sbuffer sbuf;
    msgpack::packer<msgpack::sbuffer> packer(sbuf);
    // Map with number of elements
    packer.pack_map(4);
    // 1. Protocol version
    packer.pack_uint8(static_cast<uint8_t>(Export::InverterProperty::Version));
    packer.pack_uint8(0);
    // 2. Protocol version
    packer.pack_uint8(static_cast<uint8_t>(Export::InverterProperty::DeviceType));
    packer.pack_uint8(static_cast<uint8_t>(liveData.deviceType));
    // 3. Timestamp
    packer.pack_uint8(static_cast<uint8_t>(Export::InverterProperty::Timestamp));
    uint32_t t = htonl(liveData.timestamp);
    packer.pack_ext(4, -1); // Timestamp type
    packer.pack_ext_body((const char*)(&t), 4);
    // 4. Power AC
    packer.pack_uint8(static_cast<uint8_t>(Export::InverterProperty::Power));
    packer.pack(liveData.powerTotal);

    return { sbuf.data(), sbuf.data() + sbuf.size() };
}

}
