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

#include "../Config.h"
#include "../Defines.h"
#include "../Inverter.h"
#include "../mqtt.h"
#include "../MqttMsgPackExport.h"
#include "../Timer.h"

#include <algorithm>
#include <thread>

int main()
{
    debug = 5;
    verbose = 5;

    Config config;
    config.daemon = true;
    config.liveInterval = 7;
    config.mqtt_host = "broker.hivemq.com";
    config.plantname = "testplant";
    config.mqtt_topic = "sbfspot_{serial}";
    config.mqtt_item_format = "MSGPACK";
    StringConfig array1;
    array1.name = "East";
    array1.inverterSerial = 1234567890;
    StringConfig array2;
    array2.name = "West";
    array2.inverterSerial = 1234567890;
    config.pvArrays.push_back(array1);
    config.pvArrays.push_back(array2);

    Timer timer(config);

    InverterData inverterData;
    strncpy(inverterData.DeviceName, "STP 12000TL-10", 14);
    //strncpy(inverterData.DeviceName, "123456789012345678901234567890", 30);
    inverterData.Serial = 1234567890;
    inverterData.Pmax1 = 10000;

    DayStats dayStats;
    dayStats.serial = 1234567890;
    dayStats.stringPowerMax.resize(2);

    MqttMsgPackExport mqtt(config);
    do
    {
        auto timePoint = timer.nextTimePoint();
        inverterData.ETotal = (rand()%5000)*(rand()%5000);
        inverterData.EToday = std::chrono::system_clock::to_time_t(timePoint)%100*1000;
        inverterData.Pdc1 = rand()%11111;
        inverterData.Pdc2 = rand()%12433;
        dayStats.stringPowerMax[0] += 0.5 * (inverterData.Pdc1 - dayStats.stringPowerMax[0]);
        dayStats.stringPowerMax[1] += 0.5 * (inverterData.Pdc2 - dayStats.stringPowerMax[1]);
        inverterData.TotalPac = inverterData.Pdc1 + inverterData.Pdc2 - rand()%1000;
        std::this_thread::sleep_until(timePoint);

        auto now = std::time(nullptr);
        now -= rand()%1728000;
        mqtt.exportConfig({inverterData});
        mqtt.exportDayStats(now, {dayStats});
        mqtt.exportLiveData(now, {inverterData});
    }
    while(true);

    return 0;
}
