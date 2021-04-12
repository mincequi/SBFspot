/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA solar inverters
	(c)2012-2018, SBF

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

#ifndef _SMANET_H_
#define _SMANET_H_

#include "osselect.h"

#include <vector>

extern uint16_t pcktID;
extern int packetposition;

class Buffer
{
public:
    Buffer();

    void clear();
    void writeLong(const unsigned long v);
    void writeShort(const unsigned short v);
    void writeByte(const uint8_t v);
    void writeArray(const uint8_t bytes[], const int count);
    //void writePacket(const uint8_t size, const uint8_t ctrl, const unsigned short dstSUSyID, const unsigned long dstSerial, const unsigned short packetcount, const uint8_t a, const uint8_t b, const uint8_t c);
    void writePacket(uint8_t longwords, uint8_t ctrl, unsigned short ctrl2, unsigned short dstSUSyID, unsigned long dstSerial);
    void writePacketTrailer();
    void writePacketHeader(const unsigned int control, const uint8_t *destaddress);
    void writePacketLength();
    bool validateChecksum();
    bool isCrcValid();

    std::vector<uint8_t>& data();

private:
    std::vector<uint8_t> m_data;

    friend class Inverter;
};

#endif
