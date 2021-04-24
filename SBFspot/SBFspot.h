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

#include <set>
#include <vector>

#include <SBFNet.h>
#include <Types.h>
#include <sma/SmaTypes.h>

//Wellknown SUSyID's
#define SID_MULTIGATE	175
#define SID_SB240		244

#if !defined(ARRAYSIZE)
#define ARRAYSIZE(a) sizeof(a) / sizeof(a[0])
#endif

#define tokWh(value64) (double)(value64)/1000
#define tokW(value32) (float)(value32)/1000
#define toHour(value64) (double)(value64)/3600
#define toAmp(value32) (float)value32/1000
#define toVolt(value32) (float)value32/100
#define toHz(value32) (float)value32/100
#define toTemp(value32) (float)value32/100

class Ethernet;
class Socket;
class LiveData;

class SbfSpot {
public:
    SbfSpot();

    int DaysInMonth(int month, int year);
    static int getInverterIndexByAddress(const std::vector<InverterData>& inverters, unsigned char bt_addr[6]);
    static int getInverterIndexBySerial(const std::vector<InverterData>& inverters, unsigned short SUSyID, uint32_t Serial);

    const ByteBuffer& encodeInitRequest();
    const ByteBuffer& encodeLoginRequest(uint16_t susyId, uint32_t serial, SmaUserGroup userGroup, const std::string& password);
    const ByteBuffer& encodeLoginRequest(SmaUserGroup userGroup, const std::string& password);
    const ByteBuffer& encodeLogoutRequest();
    const ByteBuffer& encodeDataRequest(uint16_t susyId, uint32_t serial, SmaInverterDataSet dataSet);
    const ByteBuffer& encodeHistoricDayDataRequest(uint16_t susyId, uint32_t serial, std::time_t from, std::time_t to, const BluetoothAddress& bluetoothAddress);
    const ByteBuffer& encodeHistoricMonthDataRequest(uint16_t susyId, uint32_t serial, std::time_t startTime, const BluetoothAddress& bluetoothAddress);

private:
    Buffer  m_buffer;
    const ByteBuffer m_emptyBuffer;
};

extern const char *IP_Inverter;
