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

struct LiveData;

class Exporter
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
    enum class Property : uint8_t {
        // Static properties
        Version = 0,    // Protocol version:    uint (max 15)
        Name = 1,       // Device name:         string (max length 23)
        StartOfProduction = 2,  // Timestamp when this inverter got installed: uint32
        Latitude = 3,   // int: degrees/180*INT_MAX, float: value in degrees
        Longitude = 4,  // fint: degrees/180*INT_MAX, float: value in degrees
        PowerMax = 5,   // rated power: any number format
        DeviceType = 6, // EnergyMeter, SolarInverter

        // Dynamic properties
        Timestamp = 8,      // Timestamp for this data set: uint32
        EnergyImportTotal = 9,    // Total consumption in Wh: any number format
        EnergyExportTotal = 10,   // Total yield in Wh: any number format
        EnergyExportToday = 11,   // Today's yield in Wh: any number format
        Power = 12,         // Current power: any number format
        PowerMaxToday = 13, // Today's maximum power: any number format
        Current = 14,       // Current in Ampere
        Voltage = 15,       // Voltage in Volt
        // Temperature = XX // in degrees celsius

        // Key for DC string or AC phase properties (stored in array of maps)
        Strings = 16,       // Data per PV array
        Phases = 17,        // Data per AC phase

        // PV array specific properties - static
        StringName = Name,
        StringAzimuth = 18,     // int16
        StringElevation = 19,   // int8
        StringPowerMax = PowerMax, // Peak power

        // Dynamic
        //StringInterval = Interval,
        StringPower = Power,    // Current power
        StringPowerMaxToday = PowerMaxToday,
        StringCurrent = Current,
        StringVoltage = Voltage,

        PropertyMax = 128 // Should be 24
    };

    enum class DeviceType : uint8_t {
        Unknown = 0,
        SolarInverter = 1,
        EnergyMeter = 2
    };

    virtual ~Exporter() = default;

    virtual ExporterType type() const;

    virtual std::string name() const;

    virtual bool open();
    virtual void close();

    /**
     * @brief Indicates whether this exporter is a "live" exporter.
     *
     * Live exporters do not store data or cause disk I/O. So, live exporters
     * can run at lower intervals without exhausting disk space.
     *
     * @return True if the exporter is a live exporter.
     */
    virtual bool isLive() const;

    // TODO: use DeviceConfig data type here (instead of InverterData).
    virtual void exportConfig(const InverterData& inverterData);
    virtual void exportDayStats(const DayStats& dayStats);
    virtual void exportLiveData(const LiveData& liveData);
    virtual void exportDayData(std::time_t timestamp,
                               const DataPerInverter& inverterData);

    // TODO: remove this obsolete functions
    virtual void exportDayData(const std::vector<InverterData>& inverters);
    virtual void exportMonthData(const std::vector<InverterData>& inverters);
    virtual void exportSpotData(std::time_t timestamp, const std::vector<InverterData>& inverters);
    virtual void exportEventData(const std::vector<InverterData>& inverters, const std::string& dt_range_csv);
    virtual void exportBatteryData(std::time_t timestamp, const std::vector<InverterData>& inverters);
};
