/************************************************************************************************
                               ____  ____  _____                _
                              / ___|| __ )|  ___|__ _ __   ___ | |_
                              \___ \|  _ \| |_ / __| '_ \ / _ \| __|
                               ___) | |_) |  _|\__ \ |_) | (_) | |_
                              |____/|____/|_|  |___/ .__/ \___/ \__|
                                                   |_|

	SBFspot - Yet another tool to read power production of SMA solar/battery inverters
	(c)2012-2021, SBF

	Latest version can be found at https://github.com/SBFspot/SBFspot

	Special Thanks to:
	S. Pittaway: Author of "NANODE SMA PV MONITOR" on which this project is based.
	W. Simons  : Early adopter, main tester and SMAdata2 Protocol analyzer
	G. Schnuff : SMAdata2 Protocol analyzer
	T. Frank   : Speedwire support
	Snowmiss   : User manual
	All other users for their contribution to the success of this project

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

	SMA, Speedwire and SMAdata2 are registered trademarks of SMA Solar Technology AG

************************************************************************************************/

#include "SBFspot.h"

#include "Ethernet.h"
#include "Import.h"

using namespace std;

SbfSpot::SbfSpot(Ethernet& ethernet, Import& import) :
    m_ethernet(ethernet),
    m_import(import)
{
}

int SbfSpot::getInverterIndexBySerial(const std::vector<InverterData>& inverters, unsigned short SUSyID, uint32_t Serial)
{
	if (DEBUG_HIGHEST)
	{
		printf("getInverterIndexBySerial()\n");
		printf("Looking up %d:%lu\n", SUSyID, (unsigned long)Serial);
	}

    uint32_t inv = 0;
    for (const auto& inverter : inverters)
    {
		if (DEBUG_HIGHEST)
            printf("Inverter[%d] %d:%lu\n", inv, inverter.SUSyID, inverter.Serial);

        if ((inverter.SUSyID == SUSyID) && inverter.Serial == Serial)
            return inv;

        ++inv;
    }

	if (DEBUG_HIGHEST)
		printf("Serial Not Found!\n");
	
	return -1;
}

//month: January = 0, February = 1...
int SbfSpot::DaysInMonth(int month, int year)
{
    const int days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    if ((month < 0) || (month > 11)) return 0;  //Error - Invalid month
    // If february, check for leap year
    if ((month == 1) && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
        return 29;
    else
        return days[month];
}

/*
 * isValidSender() compares 6-byte senderaddress with our inverter BT address
 * If senderaddress = addr_unknown (FF:FF:FF:FF:FF:FF) then any address is valid
 */
int SbfSpot::isValidSender(const unsigned char senderaddr[6], unsigned char address[6])
{
    for (int i = 0; i < 6; i++)
        if ((senderaddr[i] != address[i]) && (senderaddr[i] != 0xFF))
            return 0;

    return 1;
}

void SbfSpot::prepareInit(u_int8_t* buffer)
{
    writePacketHeader(buffer, 0, NULL);
    writePacket(buffer, 0x09, 0xA0, 0, anySUSyID, anySerial);
    writeLong(buffer, 0x00000200);
    writeLong(buffer, 0);
    writeLong(buffer, 0);
    writeLong(buffer, 0);
    writePacketLength(buffer);
}

void SbfSpot::prepareLogin(uint8_t* buffer, uint16_t susyId, uint32_t serial, SmaUserGroup userGroup, const std::string& password)
{
#define MAX_PWLENGTH 12
    unsigned char pw[MAX_PWLENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    char encChar = (userGroup == UG_USER) ? 0x88 : 0xBB;
    //Encode password
    unsigned int idx;
    for (idx = 0; idx < password.size() && (idx < sizeof(pw)); idx++)
        pw[idx] = password[idx] + encChar;
    for (; idx < MAX_PWLENGTH; idx++)
        pw[idx] = encChar;

    time_t now;

    if (ConnType == CT_BLUETOOTH)
    {
#ifdef BLUETOOTH_FOUND
        do
        {
            pcktID++;
            now = time(NULL);
            writePacketHeader(buffer, 0x01, addr_unknown);
            writePacket(buffer, 0x0E, 0xA0, 0x0100, anySUSyID, anySerial);
            writeLong(buffer, 0xFFFD040C);
            writeLong(buffer, userGroup);	// User / Installer
            writeLong(buffer, 0x00000384); // Timeout = 900sec ?
            writeLong(buffer, now);
            writeLong(buffer, 0);
            writeArray(buffer, pw, sizeof(pw));
            writePacketTrailer(buffer);
            writePacketLength(buffer);
        }
        while (!isCrcValid(buffer[packetposition-3], buffer[packetposition-2]));
#else
        if (DEBUG_NORMAL)
            std::cout << "Bluetooth not supported on this platform" << std::endl;
        return;
#endif
    }
    else    // CT_ETHERNET
    {
        do
        {
            pcktID++;
            now = time(NULL);
            writePacketHeader(buffer, 0x01, addr_unknown);
            if (susyId != SID_SB240)
                writePacket(buffer, 0x0E, 0xA0, 0x0100, susyId, serial);
            else
                writePacket(buffer, 0x0E, 0xE0, 0x0100, susyId, serial);

            writeLong(buffer, 0xFFFD040C);
            writeLong(buffer, userGroup);	// User / Installer
            writeLong(buffer, 0x00000384); // Timeout = 900sec ?
            writeLong(buffer, now);
            writeLong(buffer, 0);
            writeArray(buffer, pw, sizeof(pw));
            writePacketTrailer(buffer);
            writePacketLength(buffer);
        }
        while (!isCrcValid(buffer[packetposition-3], buffer[packetposition-2]));
    }

    return;
}

void SbfSpot::prepareLogin(std::vector<uint8_t>& buffer, SmaUserGroup userGroup, const std::string& password)
{
    buffer.resize(128);

#define MAX_PWLENGTH 12
    unsigned char pw[MAX_PWLENGTH] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

    char encChar = (userGroup == UG_USER) ? 0x88 : 0xBB;
    //Encode password
    unsigned int idx;
    for (idx = 0; idx < password.size() && (idx < sizeof(pw)); idx++)
        pw[idx] = password[idx] + encChar;
    for (; idx < MAX_PWLENGTH; idx++)
        pw[idx] = encChar;
    auto now = time(NULL);

    do
    {
        pcktID++;

        writePacketHeader(buffer.data(), 0x01, addr_unknown);
        writePacket(buffer.data(), 0x0E, 0xA0, 0x0100, 0xFFFF, 0xFFFFFFFF);
        writeLong(buffer.data(), 0xFFFD040C);
        writeLong(buffer.data(), userGroup);	// User / Installer
        writeLong(buffer.data(), 0x00000384); // Timeout = 900sec ?
        writeLong(buffer.data(), now);
        writeLong(buffer.data(), 0);
        writeArray(buffer.data(), pw, sizeof(pw));
        writePacketTrailer(buffer.data());
        writePacketLength(buffer.data());
    }
    while (!isCrcValid(buffer[packetposition-3], buffer[packetposition-2]));

    buffer.resize(packetposition);
}

void SbfSpot::prepareRequest(uint8_t* buffer, uint16_t susyId, uint32_t serial, SmaInverterDataSet dataSet)
{
    uint32_t command;
    uint32_t first;
    uint32_t last;

    switch (dataSet)
    {
    case EnergyProduction:
        // SPOT_ETODAY, SPOT_ETOTAL
        command = 0x54000200;
        first = 0x00260100;
        last = 0x002622FF;
        break;

    case SpotDCPower:
        // SPOT_PDC1, SPOT_PDC2
        command = 0x53800200;
        first = 0x00251E00;
        last = 0x00251EFF;
        break;

    case SpotDCVoltage:
        // SPOT_UDC1, SPOT_UDC2, SPOT_IDC1, SPOT_IDC2
        command = 0x53800200;
        first = 0x00451F00;
        last = 0x004521FF;
        break;

    case SpotACPower:
        // SPOT_PAC1, SPOT_PAC2, SPOT_PAC3
        command = 0x51000200;
        first = 0x00464000;
        last =  0x004642FF;
        break;

    case SpotACVoltage:
        // SPOT_UAC1, SPOT_UAC2, SPOT_UAC3, SPOT_IAC1, SPOT_IAC2, SPOT_IAC3
        command = 0x51000200;
        first = 0x00464800;
        last =  0x004655FF;
        break;

    case SpotGridFrequency:
        // SPOT_FREQ
        command = 0x51000200;
        first = 0x00465700;
        last =  0x004657FF;
        break;

    case MaxACPower:
        // INV_PACMAX1, INV_PACMAX2, INV_PACMAX3
        command = 0x51000200;
        first = 0x00411E00;
        last =  0x004120FF;
        break;

    case MaxACPower2:
        // INV_PACMAX1_2
        command = 0x51000200;
        first = 0x00832A00;
        last =  0x00832AFF;
        break;

    case SpotACTotalPower:
        // SPOT_PACTOT
        command = 0x51000200;
        first = 0x00263F00;
        last =  0x00263FFF;
        break;

    case TypeLabel:
        // INV_NAME, INV_TYPE, INV_CLASS
        command = 0x58000200;
        first = 0x00821E00;
        last = 0x008220FF;
        break;

    case SoftwareVersion:
        // INV_SWVERSION
        command = 0x58000200;
        first = 0x00823400;
        last =  0x008234FF;
        break;

    case DeviceStatus:
        // INV_STATUS
        command = 0x51800200;
        first = 0x00214800;
        last =  0x002148FF;
        break;

    case GridRelayStatus:
        // INV_GRIDRELAY
        command = 0x51800200;
        first = 0x00416400;
        last = 0x004164FF;
        break;

    case OperationTime:
        // SPOT_OPERTM, SPOT_FEEDTM
        command = 0x54000200;
        first = 0x00462E00;
        last = 0x00462FFF;
        break;

    case BatteryChargeStatus:
        command = 0x51000200;
        first = 0x00295A00;
        last = 0x00295AFF;
        break;

    case BatteryInfo:
        command = 0x51000200;
        first = 0x00491E00;
        last = 0x00495DFF;
        break;

    case InverterTemperature:
        command = 0x52000200;
        first = 0x00237700;
        last = 0x002377FF;
        break;

    case sbftest:
        command = 0x64020200;
        first = 0x00618D00;
        last = 0x00618DFF;

    case MeteringGridMsTotW:
        command = 0x51000200;
        first = 0x00463600;
        last = 0x004637FF;
        break;
    };

    do
    {
        pcktID++;
        writePacketHeader(buffer, 0x01, addr_unknown);
        if (susyId == SID_SB240)
            writePacket(buffer, 0x09, 0xE0, 0, susyId, serial);
        else
            writePacket(buffer, 0x09, 0xA0, 0, susyId, serial);
        writeLong(buffer, command);
        writeLong(buffer, first);
        writeLong(buffer, last);
        writePacketTrailer(buffer);
        writePacketLength(buffer);
    }
    while (!isCrcValid(buffer[packetposition-3], buffer[packetposition-2]));
}

void SbfSpot::decodeResponse(uint8_t* buffer, InverterData& inverter)
{
    int32_t value = 0;
    int64_t value64 = 0;
    unsigned char Vtype = 0;
    unsigned char Vbuild = 0;
    unsigned char Vminor = 0;
    unsigned char Vmajor = 0;

    int recordsize = 0;
    for (int ii = 41; ii < packetposition - 3; ii += recordsize)
    {
        uint32_t code = ((uint32_t)get_long(buffer + ii));
        LriDef lri = (LriDef)(code & 0x00FFFF00);
        uint32_t cls = code & 0xFF;
        unsigned char dataType = code >> 24;
        time_t datetime = (time_t)get_long(buffer + ii + 4);

        // fix: We can't rely on dataType because it can be both 0x00 or 0x40 for DWORDs
        if ((lri == MeteringDyWhOut) || (lri == MeteringTotWhOut) || (lri == MeteringTotFeedTms) || (lri == MeteringTotOpTms))	//QWORD
            //if ((code == SPOT_ETODAY) || (code == SPOT_ETOTAL) || (code == SPOT_FEEDTM) || (code == SPOT_OPERTM))	//QWORD
        {
            value64 = get_longlong(buffer + ii + 8);
            if ((value64 == (int64_t)NaN_S64) || (value64 == (int64_t)NaN_U64)) value64 = 0;
        }
        else if ((dataType != 0x10) && (dataType != 0x08))	//Not TEXT or STATUS, so it should be DWORD
        {
            value = (int32_t)get_long(buffer + ii + 16);
            if ((value == (int32_t)NaN_S32) || (value == (int32_t)NaN_U32)) value = 0;
        }

        switch (lri)
        {
        case GridMsTotW: //SPOT_PACTOT
            if (recordsize == 0) recordsize = 28;
            //This function gives us the time when the inverter was switched off
            inverter.SleepTime = datetime;
            inverter.TotalPac = value;
            // inverter.flags |= type;
            break;

        case OperationHealthSttOk: //INV_PACMAX1
            if (recordsize == 0) recordsize = 28;
            inverter.Pmax1 = value;
            // inverter.flags |= type;
            break;

        case OperationHealthSttWrn: //INV_PACMAX2
            if (recordsize == 0) recordsize = 28;
            inverter.Pmax2 = value;
            // inverter.flags |= type;
            break;

        case OperationHealthSttAlm: //INV_PACMAX3
            if (recordsize == 0) recordsize = 28;
            inverter.Pmax3 = value;
            // inverter.flags |= type;
            break;

        case GridMsWphsA: //SPOT_PAC1
            if (recordsize == 0) recordsize = 28;
            inverter.Pac1 = value;
            // inverter.flags |= type;
            break;

        case GridMsWphsB: //SPOT_PAC2
            if (recordsize == 0) recordsize = 28;
            inverter.Pac2 = value;
            // inverter.flags |= type;
            break;

        case GridMsWphsC: //SPOT_PAC3
            if (recordsize == 0) recordsize = 28;
            inverter.Pac3 = value;
            // inverter.flags |= type;
            break;

        case GridMsPhVphsA: //SPOT_UAC1
            if (recordsize == 0) recordsize = 28;
            inverter.Uac1 = value;
            // inverter.flags |= type;
            break;

        case GridMsPhVphsB: //SPOT_UAC2
            if (recordsize == 0) recordsize = 28;
            inverter.Uac2 = value;
            // inverter.flags |= type;
            break;

        case GridMsPhVphsC: //SPOT_UAC3
            if (recordsize == 0) recordsize = 28;
            inverter.Uac3 = value;
            // inverter.flags |= type;
            break;

        case GridMsAphsA_1: //SPOT_IAC1
        case GridMsAphsA:
            if (recordsize == 0) recordsize = 28;
            inverter.Iac1 = value;
            // inverter.flags |= type;
            break;

        case GridMsAphsB_1: //SPOT_IAC2
        case GridMsAphsB:
            if (recordsize == 0) recordsize = 28;
            inverter.Iac2 = value;
            // inverter.flags |= type;
            break;

        case GridMsAphsC_1: //SPOT_IAC3
        case GridMsAphsC:
            if (recordsize == 0) recordsize = 28;
            inverter.Iac3 = value;
            // inverter.flags |= type;
            break;

        case GridMsHz: //SPOT_FREQ
            if (recordsize == 0) recordsize = 28;
            inverter.GridFreq = value;
            // inverter.flags |= type;
            break;

        case DcMsWatt: //SPOT_PDC1 / SPOT_PDC2
            if (recordsize == 0) recordsize = 28;
            if (cls == 1)   // MPP1
            {
                inverter.Pdc1 = value;
            }
            if (cls == 2)   // MPP2
            {
                inverter.Pdc2 = value;
            }
            // inverter.flags |= type;
            break;

        case DcMsVol: //SPOT_UDC1 / SPOT_UDC2
            if (recordsize == 0) recordsize = 28;
            if (cls == 1)
            {
                inverter.Udc1 = value;
            }
            if (cls == 2)
            {
                inverter.Udc2 = value;
            }
            // inverter.flags |= type;
            break;

        case DcMsAmp: //SPOT_IDC1 / SPOT_IDC2
            if (recordsize == 0) recordsize = 28;
            if (cls == 1)
            {
                inverter.Idc1 = value;
            }
            if (cls == 2)
            {
                inverter.Idc2 = value;
            }
            // inverter.flags |= type;
            break;

        case MeteringTotWhOut: //SPOT_ETOTAL
            if (recordsize == 0) recordsize = 16;
            //In case SPOT_ETODAY missing, this function gives us inverter time (eg: SUNNY TRIPOWER 6.0)
            inverter.InverterDatetime = datetime;
            inverter.ETotal = value64;
            // inverter.flags |= type;
            break;

        case MeteringDyWhOut: //SPOT_ETODAY
            if (recordsize == 0) recordsize = 16;
            //This function gives us the current inverter time
            inverter.InverterDatetime = datetime;
            inverter.EToday = value64;
            // inverter.flags |= type;
            break;

        case MeteringTotOpTms: //SPOT_OPERTM
            if (recordsize == 0) recordsize = 16;
            inverter.OperationTime = value64;
            // inverter.flags |= type;
            break;

        case MeteringTotFeedTms: //SPOT_FEEDTM
            if (recordsize == 0) recordsize = 16;
            inverter.FeedInTime = value64;
            // inverter.flags |= type;
            break;

        case NameplateLocation: //INV_NAME
            if (recordsize == 0) recordsize = 40;
            //This function gives us the time when the inverter was switched on
            inverter.WakeupTime = datetime;
            char deviceName[33];
            strncpy(deviceName, (char *)buffer + ii + 8, sizeof(deviceName)-1);
            inverter.DeviceName = deviceName;
            // inverter.flags |= type;
            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_NAME", inverter.DeviceName.c_str(), ctime(&datetime));
            break;

        case NameplatePkgRev: //INV_SWVER
            if (recordsize == 0) recordsize = 40;
            Vtype = buffer[ii + 24];
            char ReleaseType[4];
            if (Vtype > 5)
                sprintf(ReleaseType, "%d", Vtype);
            else
                sprintf(ReleaseType, "%c", "NEABRS"[Vtype]); //NOREV-EXPERIMENTAL-ALPHA-BETA-RELEASE-SPECIAL
            Vbuild = buffer[ii + 25];
            Vminor = buffer[ii + 26];
            Vmajor = buffer[ii + 27];
            //Vmajor and Vminor = 0x12 should be printed as '12' and not '18' (BCD)
            char swVersion[16];
            snprintf(swVersion, sizeof(swVersion), "%c%c.%c%c.%02d.%s", '0'+(Vmajor >> 4), '0'+(Vmajor & 0x0F), '0'+(Vminor >> 4), '0'+(Vminor & 0x0F), Vbuild, ReleaseType);
            inverter.SWVersion = swVersion;
            // inverter.flags |= type;
            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_SWVER", inverter.SWVersion.c_str(), ctime(&datetime));
            break;

        case NameplateModel: //INV_TYPE
            if (recordsize == 0) recordsize = 40;
            for (int idx = 8; idx < recordsize; idx += 4)
            {
                unsigned long attribute = ((unsigned long)get_long(buffer + ii + idx)) & 0x00FFFFFF;
                unsigned char status = buffer[ii + idx + 3];
                if (attribute == 0xFFFFFE) break;	//End of attributes
                if (status == 1)
                {
                    std::string devtype = tagdefs.getDesc(attribute);
                    if (!devtype.empty())
                    {
                        inverter.DeviceType = devtype;
                    }
                    else
                    {
                        inverter.DeviceType = "UNKNOWN TYPE";
                        printf("Unknown Inverter Type. Report this issue at https://github.com/SBFspot/SBFspot/issues with following info:\n");
                        printf("0x%08lX and Inverter Type=<Fill in the exact type> (e.g. SB1300TL-10)\n", attribute);
                    }
                }
            }
            // inverter.flags |= type;
            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_TYPE", inverter.DeviceType.c_str(), ctime(&datetime));
            break;

        case NameplateMainModel: //INV_CLASS
            if (recordsize == 0) recordsize = 40;
            for (int idx = 8; idx < recordsize; idx += 4)
            {
                unsigned long attribute = ((unsigned long)get_long(buffer + ii + idx)) & 0x00FFFFFF;
                unsigned char attValue = buffer[ii + idx + 3];
                if (attribute == 0xFFFFFE) break;	//End of attributes
                if (attValue == 1)
                {
                    inverter.DevClass = (DEVICECLASS)attribute;
                    std::string devclass = tagdefs.getDesc(attribute);
                    if (!devclass.empty())
                    {
                        inverter.DeviceClass = devclass;
                    }
                    else
                    {
                        inverter.DeviceClass = "UNKNOWN CLASS";
                        printf("Unknown Device Class. Report this issue at https://github.com/SBFspot/SBFspot/issues with following info:\n");
                        printf("0x%08lX and Device Class=...\n", attribute);
                    }
                }
            }
            // inverter.flags |= type;
            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_CLASS", inverter.DeviceClass.c_str(), ctime(&datetime));
            break;

        case OperationHealth: //INV_STATUS:
            if (recordsize == 0) recordsize = 40;
            for (int idx = 8; idx < recordsize; idx += 4)
            {
                unsigned long attribute = ((unsigned long)get_long(buffer + ii + idx)) & 0x00FFFFFF;
                unsigned char attValue = buffer[ii + idx + 3];
                if (attribute == 0xFFFFFE) break;	//End of attributes
                if (attValue == 1)
                    inverter.DeviceStatus = attribute;
            }
            // inverter.flags |= type;
            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_STATUS", tagdefs.getDesc(inverter.DeviceStatus, "?").c_str(), ctime(&datetime));
            break;

        case OperationGriSwStt: //INV_GRIDRELAY
            if (recordsize == 0) recordsize = 40;
            for (int idx = 8; idx < recordsize; idx += 4)
            {
                unsigned long attribute = ((unsigned long)get_long(buffer + ii + idx)) & 0x00FFFFFF;
                unsigned char attValue = buffer[ii + idx + 3];
                if (attribute == 0xFFFFFE) break;	//End of attributes
                if (attValue == 1)
                    inverter.GridRelayStatus = attribute;
            }
            // inverter.flags |= type;
            if (DEBUG_NORMAL) printf("%-12s: '%s' %s", "INV_GRIDRELAY", tagdefs.getDesc(inverter.GridRelayStatus, "?").c_str(), ctime(&datetime));
            break;

        case BatChaStt:
            if (recordsize == 0) recordsize = 28;
            inverter.BatChaStt = value;
            // inverter.flags |= type;
            break;

        case BatDiagCapacThrpCnt:
            if (recordsize == 0) recordsize = 28;
            inverter.BatDiagCapacThrpCnt = value;
            // inverter.flags |= type;
            break;

        case BatDiagTotAhIn:
            if (recordsize == 0) recordsize = 28;
            inverter.BatDiagTotAhIn = value;
            // inverter.flags |= type;
            break;

        case BatDiagTotAhOut:
            if (recordsize == 0) recordsize = 28;
            inverter.BatDiagTotAhOut = value;
            // inverter.flags |= type;
            break;

        case BatTmpVal:
            if (recordsize == 0) recordsize = 28;
            inverter.BatTmpVal = value;
            // inverter.flags |= type;
            break;

        case BatVol:
            if (recordsize == 0) recordsize = 28;
            inverter.BatVol = value;
            // inverter.flags |= type;
            break;

        case BatAmp:
            if (recordsize == 0) recordsize = 28;
            inverter.BatAmp = value;
            // inverter.flags |= type;
            break;

        case CoolsysTmpNom:
            if (recordsize == 0) recordsize = 28;
            inverter.Temperature = value;
            // inverter.flags |= type;
            break;

        case MeteringGridMsTotWhOut:
            if (recordsize == 0) recordsize = 28;
            inverter.MeteringGridMsTotWOut = value;
            break;

        case MeteringGridMsTotWhIn:
            if (recordsize == 0) recordsize = 28;
            inverter.MeteringGridMsTotWIn = value;
            break;

        default:
            if (recordsize == 0) recordsize = 12;
        }
    }
}