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

#include "Types.h"

#include "Defines.h"

Serial::Serial(u_int32_t serial) :
    m_serial(serial) {
}
/*
Serial& Serial::operator=(uint32_t serial) {
    m_serial = serial;
    return *this;
}
*/

Serial::operator uint32_t() const {
    return m_serial;
}

uint32_t Serial::serial() const {
    return m_serial;
}

void ByteBuffer::reset() {
    m_currentPosition = 0;
}

uint8_t ByteBuffer::readUint8() {
    if ((m_currentPosition + 1) > size()) {
        return 0;
    }

    uint8_t value = *(data()+m_currentPosition);
    ++m_currentPosition;

    return value;
}

std::vector<uint8_t> ByteBuffer::readByteArray(uint16_t _size) {
    std::vector<uint8_t> value;

    if (_size == std::numeric_limits<uint16_t>::max()) {
        value.resize(size()-m_currentPosition);
        std::memcpy(value.data(), data()+m_currentPosition, size()-m_currentPosition);
        m_currentPosition = size();
    } else if ((m_currentPosition + _size) > size()) {
    } else {
        value.resize(_size);
        std::memcpy(value.data(), data()+m_currentPosition, _size);
        m_currentPosition += _size;
    }

    return value;
}

uint16_t ByteBuffer::readUint16(bool doByteSwap) {
    if ((m_currentPosition + 2) > size()) {
        return 0;
    }

    uint16_t value;
    std::memcpy(&value, data()+m_currentPosition, 2);
    m_currentPosition += 2;

    return doByteSwap ? ntohs(value) : value;
}

uint32_t ByteBuffer::readUint32(bool doByteSwap) {
    if ((m_currentPosition + 4) > size()) {
        return 0;
    }

    uint32_t value;
    std::memcpy(&value, data()+m_currentPosition, 4);
    m_currentPosition += 4;

    return doByteSwap ? ntohl(value) : value;
}
