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

#include "../InverterDataStorage.h"
#include "../Types.h"

#include <cassert>

int main()
{
    InverterDataStorage storage;
    InverterData data11, data12, data21, data22, data31, data32, data41, data42;
    data11.Pdc1 = 10000;
    data12.Pdc1 = 30000;
    data21.Pdc1 = 30000;
    data21.Serial = 12;
    data22.Pdc1 = 50000;
    data31.Pdc1 = 20000;
    data31.Serial = 34;
    data32.Pdc1 = 10000;
    data41.Pdc1 = 40000;
    data42.Pdc1 = 30000;

    storage.addInverterData(10, { data11, data12 });
    storage.addInverterData(20, { data21, data22 });
    storage.addInverterData(30, { data31, data32 });
    storage.addInverterData(40, { data41, data42 });

    auto result = storage.getInverterData(20, 30);
    assert(result.at(0).Pdc1 == 25000);
    assert(result.at(0).Serial == 34);
    assert(result.at(1).Pdc1 == 30000);

    result = storage.getInverterData(11, 25); // -> 20
    assert(result.at(0).Pdc1 == 30000);
    assert(result.at(0).Serial == 12);
    assert(result.at(1).Pdc1 == 50000);

    return 0;
}
