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

#include <iomanip>
#include <iostream>

#include "Ethernet.h"
#include "LiveData.h"
#include "Logger.h"
#include "SBFNet.h"
#include "Socket.h"
#include "misc.h"
#include "sma/SmaInverterRequests.h"

using namespace std;

SbfSpot::SbfSpot()
{
}

int SbfSpot::getInverterIndexByAddress(const std::vector<InverterData>& inverters, unsigned char bt_addr[6])
{
    int inv = 0;
    for (const auto& inverter : inverters)
    {
        int byt;
        for (byt=0; byt<6; byt++)
        {
            if (inverter.BTAddress[byt] != bt_addr[byt])
                break;
        }

        if (byt == 6)
            return inv;

        ++inv;
    }

    return -1;	//No inverter found
}


int SbfSpot::getInverterIndexBySerial(const std::vector<InverterData>& inverters, unsigned short SUSyID, uint32_t serial)
{
	if (DEBUG_HIGHEST)
	{
		printf("getInverterIndexBySerial()\n");
        printf("Looking up %d:%lu\n", SUSyID, (unsigned long)serial);
	}

    int inv = 0;
    for (const auto& inverter : inverters)
    {
		if (DEBUG_HIGHEST)
            printf("Inverter[%d] %d:%lu\n", inv, inverter.SUSyID, inverter.serial);

        if ((inverter.SUSyID == SUSyID) && inverter.serial == serial)
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

const ByteBuffer& SbfSpot::encodeInitRequest()
{
    m_buffer.writePacketHeader(0, BluetoothAddress());
    m_buffer.writePacket(0x09, 0xA0, 0, anySUSyID, anySerial);
    m_buffer.writeLong(0x00000200);
    m_buffer.writeLong(0);
    m_buffer.writeLong(0);
    m_buffer.writeLong(0);
    m_buffer.writePacketLength();

    return m_buffer.data();
}

const ByteBuffer& SbfSpot::encodeLoginRequest(uint16_t susyId, uint32_t serial, SmaUserGroup userGroup, const std::string& password)
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
            m_buffer.writePacketHeader(0x01, addr_unknown);
            m_buffer.writePacket(0x0E, 0xA0, 0x0100, anySUSyID, anySerial);
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
#else
        if (DEBUG_NORMAL)
            std::cout << "Bluetooth not supported on this platform" << std::endl;
        return m_emptyBuffer;
#endif
    }
    else    // CT_ETHERNET
    {
        do
        {
            pcktID++;
            now = time(NULL);
            m_buffer.writePacketHeader(0x01, addr_unknown);
            if (susyId != SID_SB240)
                m_buffer.writePacket(0x0E, 0xA0, 0x0100, susyId, serial);
            else
                m_buffer.writePacket(0x0E, 0xE0, 0x0100, susyId, serial);

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
    }

    return m_buffer.data();
}

const ByteBuffer& SbfSpot::encodeLoginRequest(SmaUserGroup userGroup, const std::string& password)
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
    auto now = time(NULL);

    do
    {
        pcktID++;

        m_buffer.writePacketHeader(0x01, addr_unknown);
        m_buffer.writePacket(0x0E, 0xA0, 0x0100, 0xFFFF, 0xFFFFFFFF);
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

    return m_buffer.data();
}

const ByteBuffer& SbfSpot::encodeLogoutRequest()
{
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

    return m_buffer.data();
}

const ByteBuffer& SbfSpot::encodeDataRequest(uint16_t susyId, uint32_t serial, SmaInverterDataSet dataSet)
{
    auto request = sma::SmaInverterRequests::create(dataSet);

    do
    {
        pcktID++;
        m_buffer.writePacketHeader(0x01, addr_unknown);
        if (susyId == SID_SB240)
            m_buffer.writePacket(0x09, 0xE0, 0, susyId, serial);
        else
            m_buffer.writePacket(0x09, 0xA0, 0, susyId, serial);
        m_buffer.writeLong(request.command);
        m_buffer.writeLong(request.first);
        m_buffer.writeLong(request.last);
        m_buffer.writePacketTrailer();
        m_buffer.writePacketLength();
    }
    while (!m_buffer.isCrcValid());

    return m_buffer.data();
}

const ByteBuffer& SbfSpot::encodeHistoricDayDataRequest(uint16_t susyId, uint32_t serial, std::time_t from, std::time_t to, const BluetoothAddress& bluetoothAddress)
{
    do
    {
        pcktID++;
        m_buffer.writePacketHeader(0x01, bluetoothAddress);
        m_buffer.writePacket(0x09, 0xE0, 0, susyId, serial);
        m_buffer.writeLong(0x70000200);
        m_buffer.writeLong(from);
        m_buffer.writeLong(to);
        m_buffer.writePacketTrailer();
        m_buffer.writePacketLength();
    }
    while (!m_buffer.isCrcValid());

    return m_buffer.data();
}

const ByteBuffer& SbfSpot::encodeHistoricMonthDataRequest(uint16_t susyId, uint32_t serial, std::time_t from, std::time_t to, const BluetoothAddress& bluetoothAddress)
{
    do
    {
        pcktID++;
        m_buffer.writePacketHeader(0x01, bluetoothAddress);
        m_buffer.writePacket(0x09, 0xE0, 0, susyId, serial);
        m_buffer.writeLong(0x70200200);
        m_buffer.writeLong(from);
        m_buffer.writeLong(to);
        m_buffer.writePacketTrailer();
        m_buffer.writePacketLength();
    }
    while (!m_buffer.isCrcValid());

    return m_buffer.data();
}
