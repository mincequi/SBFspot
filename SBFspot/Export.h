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

#include <ctime>
#include "Types.h"

class Export
{
public:
    //  - Inverter:
    //      - Version
    //      - Name
    //      - ...
    //      - PowerMax
    //      - String:
    //          - 0:
    //              - Name
    //              - PowerMax
    //              - Azimuth
    //              - Elevation
    //          - 1:
    //              - Name
    //              - PowerMax
    //              - ...
    enum class InverterProperty : uint8_t
    {
        // Static properties
        Version = 0,    // Protocol version:    uint (max 15)
        Name = 1,       // Device name:         string (max length 23)
        StartOfProduction = 2,  // Timestamp when this inverter got installed: uint32
        Latitude = 3,   // float16, float32
        Longitude = 4,  // float16, float32
        PowerMax = 5,   // uint16, uint32

        // Dynamic properties
        Timestamp = 8,      // Timestamp for this data set: uint32
        Interval = 13,      // Interval (in seconds) for this data set.
        YieldTotal = 9,     // Total yield in Wh: float16, float32, uint32
        YieldToday = 10,    // Today's yield in Wh: float16, float32, uint16, uint32
        Power = 11,         // Current power: float16, float32, uint16, uint32
        PowerMaxToday = 12, // Today's maximum power: float16, float32, uint16, uint32

        // Key for PV string properties (stored in array of maps)
        Strings = 16,       // Data per PV array: map (max length 15)

        // PV array specific properties - static
        StringName = Name,
        StringAzimuth = 17,     // int16
        StringElevation = 18,   // int8
        StringPowerMax = PowerMax, // Peak power

        // Dynamic
        StringInterval = Interval,
        StringPower = Power,    // Current power
        StringPowerMaxToday = PowerMaxToday,

        PropertyMax = 128 // Should be 24
    };

    virtual ~Export() = default;

    virtual std::string name() const = 0;

    virtual int exportConfig(const std::vector<InverterData>& inverterData);
    virtual int exportDayStats(std::time_t timestamp,
                               const std::vector<DayStats>& dayStats);
    virtual int exportLiveData(std::time_t timestamp,
                               const std::vector<InverterData>& inverterData) = 0;
    virtual int exportDayData(std::time_t timestamp,
                              const DataPerInverter& inverterData);
};
