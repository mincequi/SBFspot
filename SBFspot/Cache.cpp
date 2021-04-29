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

#include "Cache.h"

#include "Types.h"

Cache::Cache(/*Exporter& exporter*/)
    //: m_exporter(exporter)
{
}

void Cache::addInverterData(std::time_t time, const std::vector<InverterData>& inverterData)
{
    if (!m_inverterData.empty() && inverterData.size() != m_inverterData.begin()->second.size()) {
        return;
    }

    m_inverterData[time] = inverterData;
}

std::vector<InverterData> Cache::getInverterData(std::time_t startTime, std::time_t endTime)
{
    std::vector<InverterData> inverterData(m_inverterData.begin()->second.size());

    auto lower = m_inverterData.lower_bound(startTime);
    auto upper = m_inverterData.upper_bound(endTime);

    for (auto it = lower; it != upper; ++it) {
        for (size_t i = 0; i < inverterData.size(); ++i) {
            inverterData.at(i).Pdc1 += it->second.at(i).Pdc1;
            inverterData.at(i).Pdc2 += it->second.at(i).Pdc2;
            inverterData.at(i).Idc1 += it->second.at(i).Idc1;
            inverterData.at(i).Idc2 += it->second.at(i).Idc2;
            inverterData.at(i).Udc1 += it->second.at(i).Udc1;
            inverterData.at(i).Udc2 += it->second.at(i).Udc2;
            inverterData.at(i).Pac1 += it->second.at(i).Pac1;
            inverterData.at(i).Pac2 += it->second.at(i).Pac2;
            inverterData.at(i).Pac3 += it->second.at(i).Pac3;
            inverterData.at(i).Iac1 += it->second.at(i).Iac1;
            inverterData.at(i).Iac2 += it->second.at(i).Iac2;
            inverterData.at(i).Iac3 += it->second.at(i).Iac3;
            inverterData.at(i).Uac1 += it->second.at(i).Uac1;
            inverterData.at(i).Uac2 += it->second.at(i).Uac2;
            inverterData.at(i).Uac3 += it->second.at(i).Uac3;
            inverterData.at(i).GridFreq += it->second.at(i).GridFreq;
            inverterData.at(i).BT_Signal += it->second.at(i).BT_Signal;
            inverterData.at(i).Temperature += it->second.at(i).Temperature;
        }
    }

    auto dataCount = std::distance(lower, upper);
    --upper;
    for (size_t i = 0; i < inverterData.size(); ++i) {
        inverterData.at(i).Pdc1 /= dataCount;
        inverterData.at(i).Pdc2 /= dataCount;
        inverterData.at(i).Idc1 /= dataCount;
        inverterData.at(i).Idc2 /= dataCount;
        inverterData.at(i).Udc1 /= dataCount;
        inverterData.at(i).Udc2 /= dataCount;
        inverterData.at(i).Pac1 /= dataCount;
        inverterData.at(i).Pac2 /= dataCount;
        inverterData.at(i).Pac3 /= dataCount;
        inverterData.at(i).Iac1 /= dataCount;
        inverterData.at(i).Iac2 /= dataCount;
        inverterData.at(i).Iac3 /= dataCount;
        inverterData.at(i).Uac1 /= dataCount;
        inverterData.at(i).Uac2 /= dataCount;
        inverterData.at(i).Uac3 /= dataCount;
        inverterData.at(i).GridFreq /= dataCount;
        inverterData.at(i).BT_Signal /= dataCount;
        inverterData.at(i).Temperature /= dataCount;

        inverterData.at(i).Serial = upper->second.at(i).Serial;
        inverterData.at(i).EToday = upper->second.at(i).EToday;
        inverterData.at(i).ETotal = upper->second.at(i).ETotal;
        inverterData.at(i).OperationTime = upper->second.at(i).OperationTime;
        inverterData.at(i).FeedInTime = upper->second.at(i).FeedInTime;
        inverterData.at(i).DeviceStatus = upper->second.at(i).DeviceStatus;
        inverterData.at(i).GridRelayStatus = upper->second.at(i).GridRelayStatus;
    }

    return inverterData;
}

void Cache::clear()
{
    m_inverterData.clear();
}
