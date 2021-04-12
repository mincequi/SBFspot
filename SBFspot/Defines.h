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

#include "osselect.h"
#include "TagDefs.h"

// Constants
#define COMMBUFSIZE 2048 // Size of Communications Buffer (Bluetooth/Ethernet)
#define ETH_L2SIGNATURE 0x65601000

#define NaN_S32	(int32_t) 0x80000000	        // "Not a Number" representation for LONG (converted to 0 by SBFspot)
#define NaN_U32	(uint32_t)0xFFFFFFFF	        // "Not a Number" representation for ULONG (converted to 0 by SBFspot)
#define NaN_S64	(int64_t) 0x8000000000000000	// "Not a Number" representation for LONGLONG (converted to 0 by SBFspot)
#define NaN_U64	(uint64_t)0xFFFFFFFFFFFFFFFF	// "Not a Number" representation for ULONGLONG (converted to 0 by SBFspot)

static const uint32_t MAX_INVERTERS = 20;
static const uint8_t addr_unknown[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static const uint8_t addr_broadcast[6] = { 0, 0, 0, 0, 0, 0 };
static const uint16_t anySUSyID = 0xFFFF;
static const uint32_t anySerial = 0xFFFFFFFF;
static const uint16_t AppSUSyID = 125;
static const char IP_Broadcast[] = "239.12.255.254";

// Global vars
extern int debug;
extern int verbose;
extern int quiet;

extern char DateTimeFormat[32];
extern char DateFormat[32];
extern bool hasBatteryDevice;

// TODO: remove CONNECTIONTYPE and global ConnType
enum CONNECTIONTYPE {
    CT_NONE = 0,
    CT_BLUETOOTH = 1,
    CT_ETHERNET  = 2
};
extern CONNECTIONTYPE ConnType;

extern int MAX_CommBuf;
extern int MAX_pcktBuf;

extern uint8_t CommBuf[COMMBUFSIZE];
extern uint8_t pcktBuf[COMMBUFSIZE];
extern uint8_t RootDeviceAddress[6];
extern uint8_t LocalBTAddress[6];

extern unsigned long AppSerial;
extern unsigned int cmdcode;

extern TagDefs tagdefs;

extern SOCKET sock;