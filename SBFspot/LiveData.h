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

#include <cstdint>
#include <ctime>
#include <vector>

struct ElectricParameters {
    int32_t power = 0;      // [W]
    float   current = 0.0f; // [A]
    float   voltage = 0.0f; // [V]
};

struct LiveData {
    LiveData(uint32_t _serial);

    /**
     * @brief Verifies that we have valid data.
     *
     * Currently, this is more ore les for backward compatibility. If data is
     * considered valid, all necessary fields for a proper SQL export are set.
     *
     * @return True if data is valid for export.
     */
    bool isValid() const;

    void fixup();

    // Static device specific members
    uint32_t serial = 0;

    // Dynamic device specific members
    std::time_t timestamp = 0;  // [sec]

    int32_t acPowerTotal = 0;   // [W]
    int32_t dcPowerTotal = 0;   // [W]

    std::vector<ElectricParameters> ac;
    std::vector<ElectricParameters> dc;

    int64_t energyExportToday = 0;    // [Wh]
    int64_t energyExportTotal = 0;    // [Wh]
    int64_t energyImportTotal = 0;    // [Wh]
};


