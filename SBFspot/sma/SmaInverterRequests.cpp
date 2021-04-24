/************************************************************************************************
    SBFspot - Yet another tool to read power production of SMA solar inverters
    (c)2012-2021, SBF

    Latest version found at https,//github.com/SBFspot/SBFspot

    License, Attribution-NonCommercial-ShareAlike 3.0 Unported (CC BY-NC-SA 3.0)
    http,//creativecommons.org/licenses/by-nc-sa/3.0/

    You are free,
        to Share - to copy, distribute and transmit the work
        to Remix - to adapt the work
    Under the following conditions,
    Attribution,
        You must attribute the work in the manner specified by the author or licensor
        (but not in any way that suggests that they endorse you or your use of the work).
    Noncommercial,
        You may not use this work for commercial purposes.
    Share Alike,
        If you alter, transform, or build upon this work, you may distribute the resulting work
        only under the same or similar license to this one.

DISCLAIMER,
    A user of SBFspot software acknowledges that he or she is receiving this
    software on an "as is" basis and the user is not relying on the accuracy
    or functionality of the software for any purpose. The user further
    acknowledges that any use of this software will be at his own risk
    and the copyright owner accepts no responsibility whatsoever arising from
    the use or application of the software.

    SMA is a registered trademark of SMA Solar Technology AG

************************************************************************************************/

#include "SmaInverterRequests.h"

namespace sma {

SmaInverterRequest SmaInverterRequests::create(SmaInverterDataSet dataSet)
{
    for (const auto& request : m_requests) {
        if (request.dataSet == dataSet)
            return request;
    }

    return {};
}

const std::vector<SmaInverterRequest> SmaInverterRequests::m_requests = {
    { SpotDCPower,
      // SPOT_PDC1, SPOT_PDC2
      0x53800200,
      0x00251E00,
      0x00251EFF,
    },

    { EnergyProduction,
      // SPOT_ETODAY, SPOT_ETOTAL
      0x54000200,
      0x00260100,
      0x002622FF,
    },

    { SpotACTotalPower,
      // SPOT_PACTOT
      0x51000200,
      0x00263F00,
      0x00263FFF,
    },

    { MaxACPower,
      // INV_PACMAX1, INV_PACMAX2, INV_PACMAX3
      0x51000200,
      0x00411E00,
      0x004120FF,
    },

    { SpotDCVoltage,
      // SPOT_UDC1, SPOT_UDC2, SPOT_IDC1, SPOT_IDC2
      0x53800200,
      0x00451F00,
      0x004521FF,
    },

    { SpotACPower,
      // SPOT_PAC1, SPOT_PAC2, SPOT_PAC3
      0x51000200,
      0x00464000,
      0x004642FF,
    },

    { SpotACVoltage,
      // SPOT_UAC1, SPOT_UAC2, SPOT_UAC3, SPOT_IAC1, SPOT_IAC2, SPOT_IAC3
      0x51000200,
      0x00464800,
      0x004655FF,
    },

    { SpotGridFrequency,
      // SPOT_FREQ
      0x51000200,
      0x00465700,
      0x004657FF,
    },

    { MaxACPower2,
      // INV_PACMAX1_2
      0x51000200,
      0x00832A00,
      0x00832AFF,
    },

    { TypeLabel,
      // INV_NAME, INV_TYPE, INV_CLASS
      0x58000200,
      0x00821E00,
      0x008220FF,
    },

    { SoftwareVersion,
      // INV_SWVERSION
      0x58000200,
      0x00823400,
      0x008234FF,
    },

    { DeviceStatus,
      // INV_STATUS
      0x51800200,
      0x00214800,
      0x002148FF,
    },

    { GridRelayStatus,
      // INV_GRIDRELAY
      0x51800200,
      0x00416400,
      0x004164FF,
    },

    { OperationTime,
      // SPOT_OPERTM, SPOT_FEEDTM
      0x54000200,
      0x00462E00,
      0x00462FFF,
    },

    { BatteryChargeStatus,
      0x51000200,
      0x00295A00,
      0x00295AFF,
    },

    { BatteryInfo,
      0x51000200,
      0x00491E00,
      0x00495DFF,
    },

    { InverterTemperature,
      0x52000200,
      0x00237700,
      0x002377FF,
    },

    { sbftest,
      0x64020200,
      0x00618D00,
      0x00618DFF,
    },

    { MeteringGridMsTotW,
      0x51000200,
      0x00463600,
      0x004637FF,
    }
};

} // namespace sma
