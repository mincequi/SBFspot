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

#include "Logger.h"

#include <iomanip>
#include <ctime>

#include <QByteArray>

#include <Types.h>

void Logger::init(int& argc, char* argv[]) {
    /* Everything with a verbosity equal or greater than g_stderr_verbosity will be
        written to stderr. You can set this in code or via the -v argument.
        Set to loguru::Verbosity_OFF to write nothing to stderr.
        Default is 0, i.e. only log ERROR, WARNING and INFO are written to stderr.
        */
    loguru::g_stderr_verbosity  = 0;
    loguru::g_colorlogtostderr  = true; // True by default.
    loguru::g_flush_interval_ms = 0; // 0 (unbuffered) by default.
    loguru::g_preamble          = true; // Prefix each log line with date, time etc? True by default.

    /* Specify the verbosity used by loguru to log its info messages including the header
        logged when logged::init() is called or on exit. Default is 0 (INFO).
        */
    loguru::g_internal_verbosity = 3;

    loguru::g_preamble_date    = false;
    loguru::g_preamble_time    = true; // The time of the current day
    loguru::g_preamble_uptime  = false; // The time since init call
    loguru::g_preamble_thread  = false; // The logging thread
    loguru::g_preamble_file    = true; // The file from which the log originates from
    loguru::g_preamble_verbose = true; // The verbosity field
    loguru::g_preamble_pipe    = true; // The pipe symbol right before the message

    loguru::init(argc, argv);
}

Logger::Logger()
{
}

std::ostream& operator<< (std::ostream& out, const ByteBuffer& buffer)
{
    uint i = 0;
    out << "size: " << buffer.size() << ", data:" << std::endl;
    for (const uint8_t& c : buffer) {
        if (i == 0) {
            //out << std::endl;
        } else if ((i % 16) == 0) {
            out << std::endl;
        } else if ((i % 8) == 0) {
            out << " ";
        }

        out << std::uppercase << std::setfill('0') << std::setw(2) << std::hex << (uint32_t)c << " ";
        ++i;
    }

    return out;
}

std::ostream& operator<< (std::ostream& out, const DayData& dayData)
{
    return out << "timestamp: " << std::put_time(std::localtime(&dayData.datetime), "%c")
               << ", serial: " << dayData.serial
               << ", totalWh: " << dayData.totalWh
               << ", watt: " << dayData.watt;
}

std::ostream& operator<< (std::ostream& out, const Serial& serial)
{
    auto strSerial = std::to_string(serial);
    strSerial.replace(3, strSerial.length()-6, strSerial.length()-6, '*');
    return out << strSerial;
}
