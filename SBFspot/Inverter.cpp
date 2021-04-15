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

#include "Inverter.h"

#include <ctime>
#include <boost/format.hpp>

#include "ArchData.h"
#include "Config.h"
#include "CSVexport.h"
#include "Defines.h"
#include "Ethernet.h"
#include "Importer.h"
#include "SBFNet.h"
#include "SBFspot.h"
#include "TagDefs.h"
// TODO: remove bluetooth header from here. Abstract bluetooth functions using Import class
#include "bluetooth.h"
#include "misc.h"

using namespace boost;

Inverter::Inverter(const Config& config, Ethernet& ethernet, Importer& import, SbfSpot& sbfSpot)
    : m_config(config),
      m_ethernet(ethernet),
      m_import(import),
      m_sbfSpot(sbfSpot),
      m_archData(m_import),
      m_exporterManager(config, m_cache)
{
}

Inverter::~Inverter()
{
}

E_SBFSPOT Inverter::ethInitConnection()
{
    if (VERBOSE_NORMAL) puts("Initializing...");

    for (const auto& ip : m_config.ip_addresslist)
    {
        InverterData data;
        data.IPAddress = ip;
        m_inverters.push_back(data);
    }

    //Generate a Serial Number for application
    srand(time(NULL));
    AppSerial = 900000000 + ((rand() << 16) + rand()) % 100000000;
    // Fix Issue 103: Eleminate confusion: apply name: session-id iso SN
    if (VERBOSE_NORMAL) printf("SUSyID: %d - SessionID: %lu (0x%08lX)\n", AppSUSyID, AppSerial, AppSerial);

    E_SBFSPOT rc = E_OK;
    // len less than 0.0.0.0 or len of no string ==> use broadcast to detect inverters
    if (m_inverters.size() == 1 && m_inverters.front().IPAddress.size() < 8)
        m_inverters.front().IPAddress = discover();

    for (auto& inverter : m_inverters)
    {
        auto buffer = m_sbfSpot.encodeInitRequest();
        m_ethernet.ethSend(buffer, inverter.IPAddress);
        Buffer response;
        if ((rc = m_ethernet.ethGetPacket(response)) == E_OK)
        {
            const ethPacket* pckt = (ethPacket*)response.data().data();
            inverter.SUSyID = btohs(pckt->Source.SUSyID);	// Fix Issue 98
            inverter.Serial = btohl(pckt->Source.Serial);	// Fix Issue 98

            logoffSMAInverter(inverter);
        }
        else
        {
            std::cerr << "ERROR: Connection to inverter failed!\n";
            std::cerr << "Is " << inverter.IPAddress << " the correct IP?\n";
            std::cerr << "Please check IP_Address in SBFspot.cfg!\n";
        }
    }

    return rc;
}

E_SBFSPOT Inverter::logonSMAInverter(std::vector<InverterData>& inverters, long userGroup, const char *password)
{
#define MAX_PWLENGTH 12
    unsigned char pw[MAX_PWLENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    if (DEBUG_NORMAL) puts("logonSMAInverter()");

    char encChar = (userGroup == UG_USER)? 0x88:0xBB;
    //Encode password
    unsigned int idx;
    for (idx = 0; (password[idx] != 0) && (idx < sizeof(pw)); idx++)
        pw[idx] = password[idx] + encChar;
    for (; idx < MAX_PWLENGTH; idx++)
        pw[idx] = encChar;

    E_SBFSPOT rc = E_OK;
    int validPcktID = 0;

    time_t now;

    if (m_config.ConnectionType == CT_BLUETOOTH)
    {
#ifdef BLUETOOTH_FOUND
        do
        {
            pcktID++;
            now = time(NULL);
            writePacketHeader(0x01, addr_unknown);
            writePacket(0x0E, 0xA0, 0x0100, anySUSyID, anySerial);
            writeLong(0xFFFD040C);
            writeLong(userGroup);	// User / Installer
            writeLong(0x00000384); // Timeout = 900sec ?
            writeLong(now);
            writeLong(0);
            writeArray(pcktBuf, pw, sizeof(pw));
            writePacketTrailer();
            writePacketLength();
        }
        while (!isCrcValid(pcktBuf[packetposition-3], pcktBuf[packetposition-2]));

        m_import.send(pcktBuf, nullptr);

        do	//while (validPcktID == 0);
        {
            // In a multi inverter plant we get a reply from all inverters
            for (const auto& inverter : inverters)
            {
                if ((rc  = m_import.getPacket(addr_unknown, 1)) != E_OK)
                    return rc;

                if (!validateChecksum())
                    return E_CHKSUM;
                else
                {
                    unsigned short rcvpcktID = get_short(pcktBuf+27) & 0x7FFF;
                    if ((pcktID == rcvpcktID) && (get_long(pcktBuf + 41) == now))
                    {
                        int ii = m_sbfSpot.getInverterIndexByAddress(inverters, CommBuf + 4);
                        if (ii >= 0 )
                        {
                            inverters[ii].SUSyID = get_short(pcktBuf + 15);
                            inverters[ii].Serial = get_long(pcktBuf + 17);
                            validPcktID = 1;
                            unsigned short retcode = get_short(pcktBuf + 23);
                            switch (retcode)
                            {
                                case 0: rc = E_OK; break;
                                case 0x0100: rc = E_INVPASSW; break;
                                default: rc = E_LOGONFAILED; break;
                            }
                        }
                        else if (DEBUG_NORMAL) printf("Unexpected response from %02X:%02X:%02X:%02X:%02X:%02X -> ", CommBuf[9], CommBuf[8], CommBuf[7], CommBuf[6], CommBuf[5], CommBuf[4]);
                    }
                    else
                    {
                        if (DEBUG_HIGHEST) puts("Unexpected response from inverter. Let's retry...");
                    }
                }
            }
        }
        while (validPcktID == 0);
#else
        if (DEBUG_NORMAL)
            std::cout << "Bluetooth not supported on this platform" << std::endl;
        return E_COMM;
#endif
    }
    else    // CT_ETHERNET
    {
        for (const auto& inverter : inverters)
        {
            do
            {
                pcktID++;
                now = time(NULL);
                m_buffer.writePacketHeader(0x01, addr_unknown);
                if (inverter.SUSyID != SID_SB240)
                    m_buffer.writePacket(0x0E, 0xA0, 0x0100, inverter.SUSyID, inverter.Serial);
                else
                    m_buffer.writePacket(0x0E, 0xE0, 0x0100, inverter.SUSyID, inverter.Serial);

                m_buffer.writeLong(0xFFFD040C);
                m_buffer.writeLong(userGroup);	// User / Installer
                m_buffer.writeLong(0x00000384); // Timeout = 900sec ?
                m_buffer.writeLong(now);
                m_buffer.writeLong(0);
                m_buffer.writeArray(pw, sizeof(pw));
                m_buffer.writePacketTrailer();
                m_buffer.writePacketLength();
            }
            while (!m_buffer.isCrcValid());

            m_ethernet.ethSend(m_buffer.data(), inverter.IPAddress);

            validPcktID = 0;
            do
            {
                Buffer response;
                if ((rc = m_ethernet.ethGetPacket(response)) == E_OK)
                {
                    const ethPacket* pckt = (ethPacket *)response.data().data();
                    if (pcktID == (btohs(pckt->PacketID) & 0x7FFF))   // Valid Packet ID
                    {
                        validPcktID = 1;
                        unsigned short retcode = btohs(pckt->ErrorCode);
                        switch (retcode)
                        {
                            case 0: rc = E_OK; break;
                            case 0x0100: rc = E_INVPASSW; break;
                            default: rc = E_LOGONFAILED; break;
                        }
                    }
                    else
                        if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, (btohs(pckt->PacketID) & 0x7FFF));
                }
            } while ((validPcktID == 0) && (rc == E_OK)); // Fix Issue 167
        }
    }

    return rc;
}

E_SBFSPOT Inverter::logoffSMAInverter(const InverterData& inverter)
{
    if (DEBUG_NORMAL) puts("logoffSMAInverter()");
    do
    {
        pcktID++;
        m_buffer.writePacketHeader(0x01, addr_unknown);
        m_buffer.writePacket(0x08, 0xA0, 0x0300, anySUSyID, anySerial);
        m_buffer.writeLong(0xFFFD010E);
        m_buffer.writeLong(0xFFFFFFFF);
        m_buffer.writePacketTrailer();
        m_buffer.writePacketLength();
    }
    while (!m_buffer.isCrcValid());
    m_import.send(m_buffer.data(), inverter.IPAddress);

    return E_OK;
}

E_SBFSPOT Inverter::logoffMultigateDevices(const std::vector<InverterData>& inverters)
{
    if (DEBUG_NORMAL) puts("logoffMultigateDevices()");

    for (uint32_t mg = 0; mg < inverters.size(); ++mg)
    {
        const InverterData& inverter = inverters[mg];
        if (inverter.SUSyID == SID_MULTIGATE)
        {
            for (uint32_t sb240 = 0; mg < inverters.size(); ++sb240)
            {
                const InverterData& psb = inverters[sb240];
                if ((psb.SUSyID == SID_SB240) && (psb.multigateIndex == mg))
                {
                    do
                    {
                        pcktID++;
                        m_buffer.writePacketHeader(0, NULL);
                        m_buffer.writePacket(0x08, 0xE0, 0x0300, psb.SUSyID, psb.Serial);
                        m_buffer.writeLong(0xFFFD010E);
                        m_buffer.writeLong(0xFFFFFFFF);
                        m_buffer.writePacketTrailer();
                        m_buffer.writePacketLength();
                    }
                    while (!m_buffer.isCrcValid());

                    m_ethernet.ethSend(m_buffer.data(), psb.IPAddress);
                    if (VERBOSE_NORMAL)
                        std::cout << "Logoff " << psb.SUSyID << ":" << psb.Serial << std::endl;
                }
            }
        }
    }

    return E_OK;
}

E_SBFSPOT Inverter::getDeviceList(std::vector<InverterData>& inverters, int multigateIndex)
{
    E_SBFSPOT rc = E_OK;

    const int recordsize = 32;

    do
    {
        pcktID++;
        m_buffer.writePacketHeader(0x01, NULL);
        m_buffer.writePacket(0x09, 0xE0, 0, inverters[multigateIndex].SUSyID, inverters[multigateIndex].Serial);
        m_buffer.writeShort(0x0200);
        m_buffer.writeShort(0xFFF5);
        m_buffer.writeLong(0);
        m_buffer.writeLong(0xFFFFFFFF);
        m_buffer.writePacketTrailer();
        m_buffer.writePacketLength();
    }
    while (!m_buffer.isCrcValid());
    if (m_ethernet.ethSend(m_buffer.data(), inverters[multigateIndex].IPAddress) == -1)	// SOCKET_ERROR
        return E_NODATA;

    int validPcktID = 0;
    do
    {
        Buffer response;
        rc = m_ethernet.ethGetPacket(response);
        if (rc != E_OK)
            return rc;

        int16_t errorcode = get_short(response.data().data() + 23);
        if (errorcode != 0)
        {
            std::cerr << "Received errorcode=" << errorcode << std::endl;
            return (E_SBFSPOT)errorcode;
        }

        unsigned short rcvpcktID = get_short(response.data().data()+27) & 0x7FFF;
        if (pcktID == rcvpcktID)
        {
            uint32_t serial = get_long(response.data().data() + 17);
            if (serial == inverters[multigateIndex].Serial)
            {
                rc = E_NODATA;
                validPcktID = 1;
                for (int i = 41; i < packetposition - 3; i += recordsize)
                {
                    uint16_t devclass = get_short(response.data().data() + i + 4);
                    if (devclass == 3)
                    {
                        InverterData inverter;
                        inverter.SUSyID = get_short(response.data().data() + i + 6);
                        inverter.Serial = get_long(response.data().data() + i + 8);
                        inverter.IPAddress = inverters[multigateIndex].IPAddress;
                        inverter.multigateIndex = multigateIndex;
                        inverters.push_back(inverter);
                        rc = E_OK;
                    }
                }
            }
            else if (DEBUG_HIGHEST) printf("Serial Nr mismatch. Expected %lu, received %d\n", inverters[multigateIndex].Serial, serial);
        }
        else if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, rcvpcktID);
    }
    while (validPcktID == 0);

    return rc;
}

int Inverter::getInverterData(std::vector<InverterData>& inverters, SmaInverterDataSet type)
{
    if (DEBUG_NORMAL) printf("getInverterData(%d)\n", type);
    const char *strWatt = "%-12s: %ld (W) %s";
    const char *strVolt = "%-12s: %.2f (V) %s";
    const char *strAmp = "%-12s: %.3f (A) %s";
    const char *strkWh = "%-12s: %.3f (kWh) %s";
    const char *strHour = "%-12s: %.3f (h) %s";

    int rc = E_OK;

    int recordsize = 0;
    int validPcktID = 0;

    for (auto& inverter : inverters)
    {
        auto buffer = m_sbfSpot.encodeDataRequest(inverter.SUSyID, inverter.Serial, type);
        m_import.send(buffer, inverter.IPAddress);

        validPcktID = 0;
        do
        {
            rc = m_import.getPacket(m_buffer, inverter.BTAddress, 1);
            if (rc != E_OK)
                return rc;

            if ((ConnType == CT_BLUETOOTH) && (!m_buffer.validateChecksum()))
                return E_CHKSUM;

            unsigned short rcvpcktID = get_short(m_buffer.data().data()+27) & 0x7FFF;
            if (pcktID == rcvpcktID)
            {
                int inv = m_sbfSpot.getInverterIndexBySerial(inverters, get_short(m_buffer.data().data() + 15), get_long(m_buffer.data().data() + 17));
                if (inv >= 0)
                {
                    validPcktID = 1;
                    int32_t value = 0;
                    int64_t value64 = 0;
                    unsigned char Vtype = 0;
                    unsigned char Vbuild = 0;
                    unsigned char Vminor = 0;
                    unsigned char Vmajor = 0;
                    for (int ii = 41; ii < packetposition - 3; ii += recordsize)
                    {
                        uint32_t code = ((uint32_t)get_long(m_buffer.data().data() + ii));
                        LriDef lri = (LriDef)(code & 0x00FFFF00);
                        uint32_t cls = code & 0xFF;
                        unsigned char dataType = code >> 24;
                        time_t datetime = (time_t)get_long(m_buffer.data().data() + ii + 4);

                        // fix: We can't rely on dataType because it can be both 0x00 or 0x40 for DWORDs
                        if ((lri == MeteringDyWhOut) || (lri == MeteringTotWhOut) || (lri == MeteringTotFeedTms) || (lri == MeteringTotOpTms))	//QWORD
                            //if ((code == SPOT_ETODAY) || (code == SPOT_ETOTAL) || (code == SPOT_FEEDTM) || (code == SPOT_OPERTM))	//QWORD
                        {
                            value64 = get_longlong(m_buffer.data().data() + ii + 8);
                            if ((value64 == (int64_t)NaN_S64) || (value64 == (int64_t)NaN_U64)) value64 = 0;
                        }
                        else if ((dataType != 0x10) && (dataType != 0x08))	//Not TEXT or STATUS, so it should be DWORD
                        {
                            value = (int32_t)get_long(m_buffer.data().data() + ii + 16);
                            if ((value == (int32_t)NaN_S32) || (value == (int32_t)NaN_U32)) value = 0;
                        }

                        switch (lri)
                        {
                        case GridMsTotW: //SPOT_PACTOT
                            if (recordsize == 0) recordsize = 28;
                            //This function gives us the time when the inverter was switched off
                            inverters[inv].SleepTime = datetime;
                            inverters[inv].TotalPac = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strWatt, "SPOT_PACTOT", value, ctime(&datetime));
                            break;

                        case OperationHealthSttOk: //INV_PACMAX1
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Pmax1 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strWatt, "INV_PACMAX1", value, ctime(&datetime));
                            break;

                        case OperationHealthSttWrn: //INV_PACMAX2
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Pmax2 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strWatt, "INV_PACMAX2", value, ctime(&datetime));
                            break;

                        case OperationHealthSttAlm: //INV_PACMAX3
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Pmax3 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strWatt, "INV_PACMAX3", value, ctime(&datetime));
                            break;

                        case GridMsWphsA: //SPOT_PAC1
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Pac1 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strWatt, "SPOT_PAC1", value, ctime(&datetime));
                            break;

                        case GridMsWphsB: //SPOT_PAC2
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Pac2 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strWatt, "SPOT_PAC2", value, ctime(&datetime));
                            break;

                        case GridMsWphsC: //SPOT_PAC3
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Pac3 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strWatt, "SPOT_PAC3", value, ctime(&datetime));
                            break;

                        case GridMsPhVphsA: //SPOT_UAC1
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Uac1 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strVolt, "SPOT_UAC1", toVolt(value), ctime(&datetime));
                            break;

                        case GridMsPhVphsB: //SPOT_UAC2
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Uac2 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strVolt, "SPOT_UAC2", toVolt(value), ctime(&datetime));
                            break;

                        case GridMsPhVphsC: //SPOT_UAC3
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Uac3 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strVolt, "SPOT_UAC3", toVolt(value), ctime(&datetime));
                            break;

                        case GridMsAphsA_1: //SPOT_IAC1
                        case GridMsAphsA:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Iac1 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strAmp, "SPOT_IAC1", toAmp(value), ctime(&datetime));
                            break;

                        case GridMsAphsB_1: //SPOT_IAC2
                        case GridMsAphsB:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Iac2 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strAmp, "SPOT_IAC2", toAmp(value), ctime(&datetime));
                            break;

                        case GridMsAphsC_1: //SPOT_IAC3
                        case GridMsAphsC:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Iac3 = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strAmp, "SPOT_IAC3", toAmp(value), ctime(&datetime));
                            break;

                        case GridMsHz: //SPOT_FREQ
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].GridFreq = value;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf("%-12s: %.2f (Hz) %s", "SPOT_FREQ", toHz(value), ctime(&datetime));
                            break;

                        case DcMsWatt: //SPOT_PDC1 / SPOT_PDC2
                            if (recordsize == 0) recordsize = 28;
                            if (cls == 1)   // MPP1
                            {
                                inverters[inv].Pdc1 = value;
                                if (DEBUG_NORMAL) printf(strWatt, "SPOT_PDC1", value, ctime(&datetime));
                            }
                            if (cls == 2)   // MPP2
                            {
                                inverters[inv].Pdc2 = value;
                                if (DEBUG_NORMAL) printf(strWatt, "SPOT_PDC2", value, ctime(&datetime));
                            }
                            inverters[inv].flags |= type;
                            break;

                        case DcMsVol: //SPOT_UDC1 / SPOT_UDC2
                            if (recordsize == 0) recordsize = 28;
                            if (cls == 1)
                            {
                                inverters[inv].Udc1 = value;
                                if (DEBUG_NORMAL) printf(strVolt, "SPOT_UDC1", toVolt(value), ctime(&datetime));
                            }
                            if (cls == 2)
                            {
                                inverters[inv].Udc2 = value;
                                if (DEBUG_NORMAL) printf(strVolt, "SPOT_UDC2", toVolt(value), ctime(&datetime));
                            }
                            inverters[inv].flags |= type;
                            break;

                        case DcMsAmp: //SPOT_IDC1 / SPOT_IDC2
                            if (recordsize == 0) recordsize = 28;
                            if (cls == 1)
                            {
                                inverters[inv].Idc1 = value;
                                if (DEBUG_NORMAL) printf(strAmp, "SPOT_IDC1", toAmp(value), ctime(&datetime));
                            }
                            if (cls == 2)
                            {
                                inverters[inv].Idc2 = value;
                                if (DEBUG_NORMAL) printf(strAmp, "SPOT_IDC2", toAmp(value), ctime(&datetime));
                            }
                            inverters[inv].flags |= type;
                            break;

                        case MeteringTotWhOut: //SPOT_ETOTAL
                            if (recordsize == 0) recordsize = 16;
                            //In case SPOT_ETODAY missing, this function gives us inverter time (eg: SUNNY TRIPOWER 6.0)
                            inverters[inv].InverterDatetime = datetime;
                            inverters[inv].ETotal = value64;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strkWh, "SPOT_ETOTAL", tokWh(value64), ctime(&datetime));
                            break;

                        case MeteringDyWhOut: //SPOT_ETODAY
                            if (recordsize == 0) recordsize = 16;
                            //This function gives us the current inverter time
                            inverters[inv].InverterDatetime = datetime;
                            inverters[inv].EToday = value64;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strkWh, "SPOT_ETODAY", tokWh(value64), ctime(&datetime));
                            break;

                        case MeteringTotOpTms: //SPOT_OPERTM
                            if (recordsize == 0) recordsize = 16;
                            inverters[inv].OperationTime = value64;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strHour, "SPOT_OPERTM", toHour(value64), ctime(&datetime));
                            break;

                        case MeteringTotFeedTms: //SPOT_FEEDTM
                            if (recordsize == 0) recordsize = 16;
                            inverters[inv].FeedInTime = value64;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf(strHour, "SPOT_FEEDTM", toHour(value64), ctime(&datetime));
                            break;

                        case NameplateLocation: //INV_NAME
                            if (recordsize == 0) recordsize = 40;
                            //This function gives us the time when the inverter was switched on
                            inverters[inv].WakeupTime = datetime;
                            char deviceName[33];
                            strncpy(deviceName, (char *)m_buffer.data().data() + ii + 8, sizeof(deviceName)-1);
                            inverters[inv].DeviceName = deviceName;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_NAME", inverters[inv].DeviceName.c_str(), ctime(&datetime));
                            break;

                        case NameplatePkgRev: //INV_SWVER
                            if (recordsize == 0) recordsize = 40;
                            Vtype = m_buffer.data().data()[ii + 24];
                            char ReleaseType[4];
                            if (Vtype > 5)
                                sprintf(ReleaseType, "%d", Vtype);
                            else
                                sprintf(ReleaseType, "%c", "NEABRS"[Vtype]); //NOREV-EXPERIMENTAL-ALPHA-BETA-RELEASE-SPECIAL
                            Vbuild = m_buffer.data().data()[ii + 25];
                            Vminor = m_buffer.data().data()[ii + 26];
                            Vmajor = m_buffer.data().data()[ii + 27];
                            //Vmajor and Vminor = 0x12 should be printed as '12' and not '18' (BCD)
                            char swVersion[16];
                            snprintf(swVersion, sizeof(swVersion), "%c%c.%c%c.%02d.%s", '0'+(Vmajor >> 4), '0'+(Vmajor & 0x0F), '0'+(Vminor >> 4), '0'+(Vminor & 0x0F), Vbuild, ReleaseType);
                            inverters[inv].SWVersion = swVersion;
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_SWVER", inverters[inv].SWVersion.c_str(), ctime(&datetime));
                            break;

                        case NameplateModel: //INV_TYPE
                            if (recordsize == 0) recordsize = 40;
                            for (int idx = 8; idx < recordsize; idx += 4)
                            {
                                unsigned long attribute = ((unsigned long)get_long(m_buffer.data().data() + ii + idx)) & 0x00FFFFFF;
                                unsigned char status = m_buffer.data().data()[ii + idx + 3];
                                if (attribute == 0xFFFFFE) break;	//End of attributes
                                if (status == 1)
                                {
                                    std::string devtype = tagdefs.getDesc(attribute);
                                    if (!devtype.empty())
                                    {
                                        inverters[inv].DeviceType = devtype;
                                    }
                                    else
                                    {
                                        inverters[inv].DeviceType = "UNKNOWN TYPE";
                                        printf("Unknown Inverter Type. Report this issue at https://github.com/SBFspot/SBFspot/issues with following info:\n");
                                        printf("0x%08lX and Inverter Type=<Fill in the exact type> (e.g. SB1300TL-10)\n", attribute);
                                    }
                                }
                            }
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_TYPE", inverters[inv].DeviceType.c_str(), ctime(&datetime));
                            break;

                        case NameplateMainModel: //INV_CLASS
                            if (recordsize == 0) recordsize = 40;
                            for (int idx = 8; idx < recordsize; idx += 4)
                            {
                                unsigned long attribute = ((unsigned long)get_long(m_buffer.data().data() + ii + idx)) & 0x00FFFFFF;
                                unsigned char attValue = m_buffer.data().data()[ii + idx + 3];
                                if (attribute == 0xFFFFFE) break;	//End of attributes
                                if (attValue == 1)
                                {
                                    inverters[inv].DevClass = (DEVICECLASS)attribute;
                                    std::string devclass = tagdefs.getDesc(attribute);
                                    if (!devclass.empty())
                                    {
                                        inverters[inv].DeviceClass = devclass;
                                    }
                                    else
                                    {
                                        inverters[inv].DeviceClass = "UNKNOWN CLASS";
                                        printf("Unknown Device Class. Report this issue at https://github.com/SBFspot/SBFspot/issues with following info:\n");
                                        printf("0x%08lX and Device Class=...\n", attribute);
                                    }
                                }
                            }
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_CLASS", inverters[inv].DeviceClass.c_str(), ctime(&datetime));
                            break;

                        case OperationHealth: //INV_STATUS:
                            if (recordsize == 0) recordsize = 40;
                            for (int idx = 8; idx < recordsize; idx += 4)
                            {
                                unsigned long attribute = ((unsigned long)get_long(m_buffer.data().data() + ii + idx)) & 0x00FFFFFF;
                                unsigned char attValue = m_buffer.data().data()[ii + idx + 3];
                                if (attribute == 0xFFFFFE) break;	//End of attributes
                                if (attValue == 1)
                                    inverters[inv].DeviceStatus = attribute;
                            }
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_STATUS", tagdefs.getDesc(inverters[inv].DeviceStatus, "?").c_str(), ctime(&datetime));
                            break;

                        case OperationGriSwStt: //INV_GRIDRELAY
                            if (recordsize == 0) recordsize = 40;
                            for (int idx = 8; idx < recordsize; idx += 4)
                            {
                                unsigned long attribute = ((unsigned long)get_long(m_buffer.data().data() + ii + idx)) & 0x00FFFFFF;
                                unsigned char attValue = m_buffer.data().data()[ii + idx + 3];
                                if (attribute == 0xFFFFFE) break;	//End of attributes
                                if (attValue == 1)
                                    inverters[inv].GridRelayStatus = attribute;
                            }
                            inverters[inv].flags |= type;
                            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_GRIDRELAY", tagdefs.getDesc(inverters[inv].GridRelayStatus, "?").c_str(), ctime(&datetime));
                            break;

                        case BatChaStt:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].BatChaStt = value;
                            inverters[inv].flags |= type;
                            break;

                        case BatDiagCapacThrpCnt:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].BatDiagCapacThrpCnt = value;
                            inverters[inv].flags |= type;
                            break;

                        case BatDiagTotAhIn:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].BatDiagTotAhIn = value;
                            inverters[inv].flags |= type;
                            break;

                        case BatDiagTotAhOut:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].BatDiagTotAhOut = value;
                            inverters[inv].flags |= type;
                            break;

                        case BatTmpVal:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].BatTmpVal = value;
                            inverters[inv].flags |= type;
                            break;

                        case BatVol:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].BatVol = value;
                            inverters[inv].flags |= type;
                            break;

                        case BatAmp:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].BatAmp = value;
                            inverters[inv].flags |= type;
                            break;

                        case CoolsysTmpNom:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].Temperature = value;
                            inverters[inv].flags |= type;
                            break;

                        case MeteringGridMsTotWhOut:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].MeteringGridMsTotWOut = value;
                            break;

                        case MeteringGridMsTotWhIn:
                            if (recordsize == 0) recordsize = 28;
                            inverters[inv].MeteringGridMsTotWIn = value;
                            break;

                        default:
                            if (recordsize == 0) recordsize = 12;
                        }
                    }
                }
            }
            else
            {
                if (DEBUG_HIGHEST) printf("Packet ID mismatch. Expected %d, received %d\n", pcktID, rcvpcktID);
            }
        }
        while (validPcktID == 0);
    }

    return E_OK;
}

int Inverter::process(std::time_t timestamp)
{
    int rc = logOn();
    if (rc != 0)
    {
        logOff();
        return rc;
    }

    if (VERBOSE_NORMAL) puts("Logon OK");

#ifdef BLUETOOTH_FOUND
    // If SBFspot is executed with -settime argument
    if (m_config.settime == 1)
    {
        rc = bthSetPlantTime(0, 0, 0);	// Set time ignoring limits
        logoffSMAInverter(m_inverters[0]);
        logOff();
        m_import.close();	// Close socket

        return rc;
    }

    // Synchronize plant time with system time
    // Only BT connected devices and if enabled in config _or_ requested by 123Solar
    // Most probably Speedwire devices get their time from the local IP network
    if ((m_config.ConnectionType == CT_BLUETOOTH) && (m_config.synchTime > 0 || m_config.s123 == S123_SYNC ))
        if ((rc = bthSetPlantTime(m_config.synchTime, m_config.synchTimeLow, m_config.synchTimeHigh)) != E_OK)
            std::cerr << "bthSetPlantTime returned an error: " << rc << std::endl;
#endif

    // Import Spot Data
    rc = importSpotData(timestamp);
    if (rc != 0) {
        std::cerr << "Importing live data failed." << std::endl;
    }

    // Export Config
    for (const auto& inverter : m_inverters) {
        m_exporterManager.exportConfig(inverter);
    }

    // Export Spot Data
    m_exporterManager.exportSpotData(timestamp, m_inverters);
    m_cache.clear();

    // Only export archive data, when not running in daemon mode OR
    // when in daemon mode and current timestamp matches archive interval.
    if (!m_config.daemon ||
            (m_config.archiveInterval > 0 &&
            (timestamp % m_config.archiveInterval == 0)))
    {
        importDayData();
        importMonthData();
        importEventData();
    }

    logOff();
    m_import.close();
    m_exporterManager.close();

    return rc;
}

void Inverter::reset()
{
    m_dayStats.clear();
    m_dayStats.resize(m_inverters.size());
    auto now = std::time(nullptr);
    for (size_t i = 0; i < m_inverters.size(); ++i) {
        m_dayStats[i].serial = m_inverters[i].Serial;
        m_dayStats[i].timestamp = now;
        m_exporterManager.exportDayStats(m_dayStats[i]);
    }
}

std::string Inverter::discover()
{
    // Start with UDP broadcast to check for SMA devices on the LAN
    m_buffer.data().resize(20);
    m_buffer.writeLong(0x00414D53);  //Start of SMA header
    m_buffer.writeLong(0xA0020400);  //Unknown
    m_buffer.writeLong(0xFFFFFFFF);  //Unknown
    m_buffer.writeLong(0x20000000);  //Unknown
    m_buffer.writeLong(0x00000000);  //Unknown

    m_ethernet.ethSend(m_buffer.data(), IP_Broadcast);

    //SMA inverter announces its presence in response to the discovery request packet
    auto response = m_ethernet.ethRead();

    // if bytesRead == 0, no data was received
    if (response.empty())
    {
        std::cerr << "ERROR: No inverter responded to identification broadcast.\n";
        std::cerr <<	"Try to set IP_Address in SBFspot.cfg!\n";
        return {};
    }

    if (DEBUG_NORMAL) HexDump(response, 10);

    char ipAddress[20];
    sprintf(ipAddress, "%d.%d.%d.%d", response.data()[38], response.data()[39], response.data()[40], response.data()[41]);

    return ipAddress;
}

int Inverter::logOn()
{
    char msg[80];
    int rc = 0;

    if (m_config.ConnectionType == CT_BLUETOOTH)
    {
#ifdef BLUETOOTH_FOUND
        int attempts = 1;
        do
        {
            if (attempts != 1) sleep(1);
            {
                if (VERBOSE_NORMAL) printf("Connecting to %s (%d/%d)\n", m_config.BT_Address, attempts, m_config.BT_ConnectRetries);
                rc = bthConnect(m_config.BT_Address);
            }
            attempts++;
        }
        while ((attempts <= m_config.BT_ConnectRetries) && (rc != 0));


        if (rc != 0)
        {
            snprintf(msg, sizeof(msg), "bthConnect() returned %d\n", rc);
            print_error(stdout, PROC_CRITICAL, msg);
            return rc;
        }

        rc = bthInitConnection(m_config.BT_Address, m_inverters, m_config.MIS_Enabled);
        if (rc != E_OK)
        {
            print_error(stdout, PROC_CRITICAL, "Failed to initialize communication with inverter.\n");
            m_import.close();
            return rc;
        }
        else
        {
            logoffSMAInverter(m_inverters[0]);
        }

        rc = bthGetSignalStrength(m_inverters[0]);
        if (VERBOSE_NORMAL) printf("BT Signal=%0.1f%%\n", m_inverters[0].BT_Signal);
#else
        std::cerr << "Bluetooth not supported on this platform" << std::endl;
        return E_COMM;
#endif
    }
    else // CT_ETHERNET
    {
        if (VERBOSE_NORMAL) printf("Connecting to Local Network...\n");
        rc = m_ethernet.ethConnect(m_config.IP_Port);
        if (rc != 0)
        {
            print_error(stdout, PROC_CRITICAL, "Failed to set up socket connection.");
            return rc;
        }

        rc = ethInitConnection();
        if (rc != E_OK)
        {
            print_error(stdout, PROC_CRITICAL, "Failed to initialize Speedwire connection.");
            m_ethernet.ethClose();
            return rc;
        }
    }

    if (logonSMAInverter(m_inverters, m_config.userGroup, m_config.SMA_Password) != E_OK)
    {
        snprintf(msg, sizeof(msg), "Logon failed. Check '%s' Password\n", m_config.userGroup == UG_USER? "USER":"INSTALLER");
        print_error(stdout, PROC_CRITICAL, msg);
        m_import.close();
        return 1;
    }

    /*************************************************
     * At this point we are logged on to the inverter
     *************************************************/

    return rc;
}

void Inverter::logOff()
{
    if (m_config.ConnectionType == CT_BLUETOOTH)
        logoffSMAInverter(m_inverters[0]);
    else
    {
        logoffMultigateDevices(m_inverters);
        for (const auto& inverter : m_inverters)
            logoffSMAInverter(inverter);
        m_ethernet.ethClose();
    }

    m_inverters.clear();
}

int Inverter::importSpotData(std::time_t timestamp)
{
    int rc = 0;
    if ((rc = getInverterData(m_inverters, sbftest)) != 0)
        std::cerr << "getInverterData(sbftest) returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, SoftwareVersion)) != 0)
        std::cerr << "getSoftwareVersion returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, TypeLabel)) != 0)
        std::cerr << "getTypeLabel returned an error: " << rc << std::endl;
    else
    {
        for (auto& inverter : m_inverters)
        {
            if ((inverter.DevClass == BatteryInverter) || (inverter.SUSyID == 292))	//SB 3600-SE (Smart Energy)
                hasBatteryDevice = inverter.hasBattery = true;
            else
                inverter.hasBattery = false;

            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                printf("Device Name:      %s\n", inverter.DeviceName.c_str());
                printf("Device Class:     %s%s\n", inverter.DeviceClass.c_str(), (inverter.SUSyID == 292) ? " (with battery)":"");
                printf("Device Type:      %s\n", inverter.DeviceType.c_str());
                printf("Software Version: %s\n", inverter.SWVersion.c_str());
                printf("Serial number:    %lu\n", inverter.Serial);
            }
        }
    }

    // Check for Multigate and get connected devices
    uint32_t multigateIndex = 0;
    for (auto& inverter : m_inverters)
    {
        if ((inverter.DevClass == CommunicationProduct) && (inverter.SUSyID == SID_MULTIGATE))
        {
            if (VERBOSE_HIGH)
                std::cout << "Multigate found. Looking for connected devices..." << std::endl;

            // multigate has its own ID
            inverter.multigateIndex = multigateIndex;

            if ((rc = getDeviceList(m_inverters, multigateIndex)) != 0)
                std::cout << "getDeviceList returned an error: " << rc << std::endl;
            else
            {
                if (VERBOSE_HIGH)
                {
                    std::cout << "Found these devices:" << std::endl;
                    uint32_t id = 0;
                    for (auto& inverter : m_inverters)
                    {
                        std::cout << "ID:" << id << " S/N:" << inverter.SUSyID << "-" << inverter.Serial << " IP:" << inverter.IPAddress << std::endl;
                        ++id;
                    }
                }

                if (logonSMAInverter(m_inverters, m_config.userGroup, m_config.SMA_Password) != E_OK)
                {
                    char msg[80];
                    snprintf(msg, sizeof(msg), "Logon failed. Check '%s' Password\n", m_config.userGroup == UG_USER? "USER":"INSTALLER");
                    print_error(stdout, PROC_CRITICAL, msg);
                    logOff();
                    m_ethernet.ethClose();
                    return 1;
                }

                if ((rc = getInverterData(m_inverters, SoftwareVersion)) != 0)
                    printf("getSoftwareVersion returned an error: %d\n", rc);

                if ((rc = getInverterData(m_inverters, TypeLabel)) != 0)
                    printf("getTypeLabel returned an error: %d\n", rc);
                else
                {
                    if (VERBOSE_NORMAL)
                    {
                        for (const auto& inverter : m_inverters) {
                            printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                            printf("Device Name:      %s\n", inverter.DeviceName.c_str());
                            printf("Device Class:     %s\n", inverter.DeviceClass.c_str());
                            printf("Device Type:      %s\n", inverter.DeviceType.c_str());
                            printf("Software Version: %s\n", inverter.SWVersion.c_str());
                            printf("Serial number:    %lu\n", inverter.Serial);
                        }
                    }
                }
            }
        }
        ++multigateIndex;
    }

    if (hasBatteryDevice)
    {
        if ((rc = getInverterData(m_inverters, BatteryChargeStatus)) != 0)
            std::cerr << "getBatteryChargeStatus returned an error: " << rc << std::endl;
        else
        {
            for (const auto& inverter : m_inverters)
            {
                if ((inverter.DevClass == BatteryInverter) || (inverter.hasBattery))
                {
                    if (VERBOSE_NORMAL)
                    {
                        printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                        printf("Batt. Charging Status: %lu%%\n", inverter.BatChaStt);
                    }
                }
            }
        }

        if ((rc = getInverterData(m_inverters, BatteryInfo)) != 0)
            std::cerr << "getBatteryInfo returned an error: " << rc << std::endl;
        else
        {
            for (const auto& inverter : m_inverters)
            {
                if ((inverter.DevClass == BatteryInverter) || (inverter.hasBattery))
                {
                    if (VERBOSE_NORMAL)
                    {
                        printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                        printf("Batt. Temperature: %3.1f%s\n", (float)(inverter.BatTmpVal / 10), tagdefs.getDesc(tagdefs.DEG_C).c_str());
                        printf("Batt. Voltage    : %3.2fV\n", toVolt(inverter.BatVol));
                        printf("Batt. Current    : %2.3fA\n", toAmp(inverter.BatAmp));
                    }
                }
            }
        }

        if ((rc = getInverterData(m_inverters, MeteringGridMsTotW)) != 0)
            std::cerr << "getMeteringGridInfo returned an error: " << rc << std::endl;
        else
        {
            for (const auto& inverter : m_inverters)
            {
                if ((inverter.DevClass == BatteryInverter) || (inverter.hasBattery))
                {
                    if (VERBOSE_NORMAL)
                    {
                        printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                        printf("Grid Power Out : %dW\n", inverter.MeteringGridMsTotWOut);
                        printf("Grid Power In  : %dW\n", inverter.MeteringGridMsTotWIn);
                    }
                }
            }
        }
    }

    if ((rc = getInverterData(m_inverters, DeviceStatus)) != 0)
        std::cerr << "getDeviceStatus returned an error: " << rc << std::endl;
    else
    {
        for (const auto& inverter : m_inverters)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                printf("Device Status:      %s\n", tagdefs.getDesc(inverter.DeviceStatus, "?").c_str());
            }
        }
    }

    if ((rc = getInverterData(m_inverters, InverterTemperature)) != 0)
        std::cerr << "getInverterTemperature returned an error: " << rc << std::endl;
    else
    {
        for (const auto& inverter : m_inverters)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                printf("Device Temperature: %3.1f%s\n", ((float)inverter.Temperature / 100), tagdefs.getDesc(tagdefs.DEG_C).c_str());
            }
        }
    }

    if (m_inverters[0].DevClass == SolarInverter)
    {
        if ((rc = getInverterData(m_inverters, GridRelayStatus)) != 0)
            std::cerr << "getGridRelayStatus returned an error: " << rc << std::endl;
        else
        {
            for (const auto& inverter : m_inverters)
            {
                if (inverter.DevClass == SolarInverter)
                {
                    if (VERBOSE_NORMAL)
                    {
                        printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                        printf("GridRelay Status:      %s\n", tagdefs.getDesc(inverter.GridRelayStatus, "?").c_str());
                    }
                }
            }
        }
    }

    if ((rc = getInverterData(m_inverters, MaxACPower)) != 0)
        std::cerr << "getMaxACPower returned an error: " << rc << std::endl;
    else
    {
        //TODO: REVIEW THIS PART (getMaxACPower & getMaxACPower2 should be 1 function)
        if ((m_inverters[0].Pmax1 == 0) && (rc = getInverterData(m_inverters, MaxACPower2)) != 0)
            std::cerr << "getMaxACPower2 returned an error: " << rc << std::endl;
        else
        {
            for (const auto& inverter : m_inverters)
            {
                if (VERBOSE_NORMAL)
                {
                    printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                    printf("Pac max phase 1: %luW\n", inverter.Pmax1);
                    printf("Pac max phase 2: %luW\n", inverter.Pmax2);
                    printf("Pac max phase 3: %luW\n", inverter.Pmax3);
                }
            }
        }
    }

    if ((rc = getInverterData(m_inverters, EnergyProduction)) != 0)
        std::cerr << "getEnergyProduction returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, OperationTime)) != 0)
        std::cerr << "getOperationTime returned an error: " << rc << std::endl;
    else
    {
        for (const auto& inverter : m_inverters)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                puts("Energy Production:");
                printf("\tEToday: %.3fkWh\n", tokWh(inverter.EToday));
                printf("\tETotal: %.3fkWh\n", tokWh(inverter.ETotal));
                printf("\tOperation Time: %.2fh\n", toHour(inverter.OperationTime));
                printf("\tFeed-In Time  : %.2fh\n", toHour(inverter.FeedInTime));
            }
        }
    }

    if ((rc = getInverterData(m_inverters, SpotDCPower)) != 0)
        std::cerr << "getSpotDCPower returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, SpotDCVoltage)) != 0)
        std::cerr << "getSpotDCVoltage returned an error: " << rc << std::endl;

    //Calculate missing DC Spot Values
    if (m_config.calcMissingSpot == 1)
        CalcMissingSpot(m_inverters[0]);

    for (auto& inverter : m_inverters)
    {
        inverter.calPdcTot = inverter.Pdc1 + inverter.Pdc2;
        if (VERBOSE_NORMAL)
        {
            printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
            puts("DC Spot Data:");
            printf("\tString 1 Pdc: %7.3fkW - Udc: %6.2fV - Idc: %6.3fA\n", tokW(inverter.Pdc1), toVolt(inverter.Udc1), toAmp(inverter.Idc1));
            printf("\tString 2 Pdc: %7.3fkW - Udc: %6.2fV - Idc: %6.3fA\n", tokW(inverter.Pdc2), toVolt(inverter.Udc2), toAmp(inverter.Idc2));
            printf("\tCalculated Total Pdc: %7.3fkW\n", tokW(inverter.calPdcTot));
        }
    }

    if ((rc = getInverterData(m_inverters, SpotACPower)) != 0)
        std::cerr << "getSpotACPower returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, SpotACVoltage)) != 0)
        std::cerr << "getSpotACVoltage returned an error: " << rc << std::endl;

    if ((rc = getInverterData(m_inverters, SpotACTotalPower)) != 0)
        std::cerr << "getSpotACTotalPower returned an error: " << rc << std::endl;

    //Calculate missing AC Spot Values
    if (m_config.calcMissingSpot == 1)
        CalcMissingSpot(m_inverters[0]);

    for (auto& inverter : m_inverters)
    {
        inverter.calPacTot = inverter.Pac1 + inverter.Pac2 + inverter.Pac3;
        //Calculated Inverter Efficiency
        inverter.calEfficiency = inverter.calPdcTot == 0 ? 0.0f : 100.0f * (float)inverter.calPacTot / (float)inverter.calPdcTot;
        if (VERBOSE_NORMAL)
        {
            printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
            puts("AC Spot Data:");
            printf("\tPhase 1 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(inverter.Pac1), toVolt(inverter.Uac1), toAmp(inverter.Iac1));
            printf("\tPhase 2 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(inverter.Pac2), toVolt(inverter.Uac2), toAmp(inverter.Iac2));
            printf("\tPhase 3 Pac : %7.3fkW - Uac: %6.2fV - Iac: %6.3fA\n", tokW(inverter.Pac3), toVolt(inverter.Uac3), toAmp(inverter.Iac3));
            printf("\tTotal Pac   : %7.3fkW - Calculated Pac: %7.3fkW\n", tokW(inverter.TotalPac), tokW(inverter.calPacTot));
            printf("\tEfficiency  : %7.2f%%\n", inverter.calEfficiency);
        }
    }

    if ((rc = getInverterData(m_inverters, SpotGridFrequency)) != 0)
        std::cerr << "getSpotGridFrequency returned an error: " << rc << std::endl;
    else
    {
        for (const auto& inverter : m_inverters)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                printf("Grid Freq. : %.2fHz\n", toHz(inverter.GridFreq));
            }
        }
    }

    if (m_inverters[0].DevClass == SolarInverter)
    {
        for (const auto& inverter : m_inverters)
        {
            if (VERBOSE_NORMAL)
            {
                printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                if (inverter.InverterDatetime > 0)
                    printf("Current Inverter Time: %s\n", strftime_t(m_config.DateTimeFormat, inverter.InverterDatetime));

                if (inverter.WakeupTime > 0)
                    printf("Inverter Wake-Up Time: %s\n", strftime_t(m_config.DateTimeFormat, inverter.WakeupTime));

                if (inverter.SleepTime > 0)
                    printf("Inverter Sleep Time  : %s\n", strftime_t(m_config.DateTimeFormat, inverter.SleepTime));
            }
        }
    }

    m_cache.addInverterData(timestamp, m_inverters);

    return 0;
}

void Inverter::importDayData()
{
    int rc = 0;
    //SolarInverter -> Continue to get archive data
    unsigned int idx;

    /***************
    * Get Day Data *
    ****************/
    time_t arch_time = (0 == m_config.startdate) ? time(NULL) : m_config.startdate;

    for (int count=0; count<m_config.archDays; count++)
    {
        if ((rc = m_archData.ArchiveDayData(m_inverters, arch_time)) != E_OK)
        {
            if (rc != E_ARCHNODATA)
                std::cerr << "ArchiveDayData returned an error: " << rc << std::endl;
        }
        else
        {
            if (VERBOSE_HIGH)
            {
                for (const auto& inverter : m_inverters)
                {
                    printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                    for (idx=0; idx<sizeof(inverter.dayData)/sizeof(DayData); idx++)
                        if (inverter.dayData[idx].datetime > 0)
                        {
                            printf("%s : %.3fkWh - %3.3fW\n", strftime_t(m_config.DateTimeFormat, inverter.dayData[idx].datetime), (double)inverter.dayData[idx].totalWh/1000, (double)inverter.dayData[idx].watt);
                            fflush(stdout);
                        }
                    puts("======");
                }
            }

            m_exporterManager.exportDayData(m_inverters);
        }

        //Goto previous day
        arch_time -= 86400;
    }
}

void Inverter::importMonthData()
{
    /*****************
    * Get Month Data *
    ******************/
    if (m_config.archMonths > 0)
    {
        m_archData.getMonthDataOffset(m_inverters); //Issues 115/130
        time_t arch_time = (0 == m_config.startdate) ? time(NULL) : m_config.startdate;
        struct tm arch_tm;
        memcpy(&arch_tm, gmtime(&arch_time), sizeof(arch_tm));

        for (int count=0; count<m_config.archMonths; count++)
        {
            m_archData.ArchiveMonthData(m_inverters, &arch_tm);

            if (VERBOSE_HIGH)
            {
                for (const auto& inverter : m_inverters)
                {
                    printf("SUSyID: %d - SN: %lu\n", inverter.SUSyID, inverter.Serial);
                    for (unsigned int ii = 0; ii < sizeof(inverter.monthData) / sizeof(MonthData); ii++)
                        if (inverter.monthData[ii].datetime > 0)
                            printf("%s : %.3fkWh - %3.3fkWh\n", strfgmtime_t(m_config.DateFormat, inverter.monthData[ii].datetime), (double)inverter.monthData[ii].totalWh / 1000, (double)inverter.monthData[ii].dayWh / 1000);
                    puts("======");
                }
            }

            m_exporterManager.exportMonthData(m_inverters);

            //Go to previous month
            if (--arch_tm.tm_mon < 0)
            {
                arch_tm.tm_mon = 11;
                arch_tm.tm_year--;
            }
        }
    }
}

void Inverter::importEventData()
{
    int rc = 0;
    /*****************
    * Get Event Data *
    ******************/
    posix_time::ptime tm_utc(posix_time::from_time_t((0 == m_config.startdate) ? time(NULL) : m_config.startdate));
    //ptime tm_utc(posix_time::second_clock::universal_time());
    gregorian::date dt_utc(tm_utc.date().year(), tm_utc.date().month(), 1);
    std::string dt_range_csv = str(format("%d%02d") % dt_utc.year() % static_cast<short>(dt_utc.month()));

    for (int m = 0; m < m_config.archEventMonths; m++)
    {
        if (VERBOSE_LOW)
            std::cout << "Reading events: " << to_simple_string(dt_utc) << std::endl;
        //Get user level events
        rc = m_archData.ArchiveEventData(m_inverters, dt_utc, UG_USER);
        if (rc == E_EOF) break; // No more data (first event reached)
        else if (rc != E_OK) std::cerr << "ArchiveEventData(user) returned an error: " << rc << std::endl;

        //When logged in as installer, get installer level events
        if (m_config.userGroup == UG_INSTALLER)
        {
            rc = m_archData.ArchiveEventData(m_inverters, dt_utc, UG_INSTALLER);
            if (rc == E_EOF) break; // No more data (first event reached)
            else if (rc != E_OK) std::cerr << "ArchiveEventData(installer) returned an error: " << rc << std::endl;
        }

        //Move to previous month
        if (dt_utc.month() == 1)
            dt_utc = gregorian::date(dt_utc.year() - 1, 12, 1);
        else
            dt_utc = gregorian::date(dt_utc.year(), dt_utc.month() - 1, 1);

    }

    if (rc == E_OK)
    {
        //Adjust start of range with 1 month
        if (dt_utc.month() == 12)
            dt_utc = gregorian::date(dt_utc.year() + 1, 1, 1);
        else
            dt_utc = gregorian::date(dt_utc.year(), dt_utc.month() + 1, 1);
    }

    if ((rc == E_OK) || (rc == E_EOF))
    {
        dt_range_csv = str(format("%d%02d-%s") % dt_utc.year() % static_cast<short>(dt_utc.month()) % dt_range_csv);
        m_exporterManager.exportEventData(m_inverters, dt_range_csv);
    }
}

//Power Values are missing on some inverters
void Inverter::CalcMissingSpot(InverterData& invData)
{
    if (invData.Pdc1 == 0) invData.Pdc1 = (invData.Idc1 * invData.Udc1) / 100000;
    if (invData.Pdc2 == 0) invData.Pdc2 = (invData.Idc2 * invData.Udc2) / 100000;

    if (invData.Pac1 == 0) invData.Pac1 = (invData.Iac1 * invData.Uac1) / 100000;
    if (invData.Pac2 == 0) invData.Pac2 = (invData.Iac2 * invData.Uac2) / 100000;
    if (invData.Pac3 == 0) invData.Pac3 = (invData.Iac3 * invData.Uac3) / 100000;

    if (invData.TotalPac == 0) invData.TotalPac = invData.Pac1 + invData.Pac2 + invData.Pac3;
}
