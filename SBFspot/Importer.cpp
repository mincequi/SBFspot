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

#include "Importer.h"

#ifdef BLUETOOTH_FOUND
#include "bluetooth.h"
#endif

#include "Config.h"
#include "Ethernet.h"
#include "SBFspot.h"

Importer::Importer(const Config& config, Ethernet& ethernet) :
    m_config(config),
    m_ethernet(ethernet)
{
}

Importer::~Importer()
{
}

int Importer::close()
{
#ifdef BLUETOOTH_FOUND
    return bthClose();
#else
    return 0;
#endif
}

E_SBFSPOT Importer::getPacket(Buffer& buffer, const unsigned char senderaddr[6], int wait4Command)
{
    if (m_config.ConnectionType == CT_BLUETOOTH)
    {
#ifdef BLUETOOTH_FOUND
        return bthGetPacket(buffer, senderaddr, wait4Command);
#else
        std::cout << "Bluetooth not supported on this platform" << std::endl;
        return {};
#endif
    }
    else
    {
        return m_ethernet.ethGetPacket(buffer);
    }
}

int Importer::send(const ByteBuffer& buffer, const std::string& toIP)
{
    if (m_config.ConnectionType == CT_BLUETOOTH)
    {
#ifdef BLUETOOTH_FOUND
        return bthSend(buffer);
#else
        std::cout << "Bluetooth not supported on this platform" << std::endl;
        return 0;
#endif
    }
    else
    {
        return m_ethernet.ethSend(buffer, toIP);
    }
}
