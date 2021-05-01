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

#include "SqlQueries.h"

#include <LiveData.h>
#include <Logger.h>
#include <Types.h>

namespace sql {

std::string SqlQueries::createTableLiveDataAc() {
    return "CREATE Table LiveDataAc ("
           "TimeStamp INT UNSIGNED NOT NULL,"
           "Serial INT UNSIGNED NOT NULL,"
           "P1 int, I1 float, U1 float,"
           "P2 int, I2 float, U2 float,"
           "P3 int, I3 float, U3 float,"
           "ETodayExport int(8), ETotalExport int(8), ETotalImport int(8),"
           "PRIMARY KEY (TimeStamp, Serial)"
           ")";
}

std::string SqlQueries::createTableLiveDataDc() {
    return "CREATE TABLE LiveDataDc ("
           "TimeStamp INT UNSIGNED NOT NULL,"
           "Serial INT UNSIGNED NOT NULL,"
           "Cls int NOT NULL,"
           "P int, I float, U float,"
           "PRIMARY KEY (TimeStamp, Serial, Cls)"
           ")";
}

std::string SqlQueries::createTableLiveDataAux() {
    return "CREATE TABLE LiveDataAux ("
           "TimeStamp INT UNSIGNED NOT NULL,"
           "Serial INT UNSIGNED NOT NULL,"
           "`Key` int(4) NOT NULL,"
           "Value int(4),"
           "PRIMARY KEY (TimeStamp, Serial, `Key`)"
           ")";
}

std::string SqlQueries::createTableDayData() {
    return "CREATE TABLE DayData ("
           "TimeStamp INT UNSIGNED NOT NULL,"
           "Serial INT UNSIGNED NOT NULL,"
           "TotalYield int(8),"
           "Power int(8),"
           "PRIMARY KEY (TimeStamp, Serial)"
           ")";
}

std::string SqlQueries::createTableMonthData() {
    return "CREATE TABLE MonthData ("
           "TimeStamp INT UNSIGNED NOT NULL,"
           "Serial INT UNSIGNED NOT NULL,"
           "TotalYield int(8),"
           "DayYield int(8),"
           "PRIMARY KEY (TimeStamp, Serial)"
           ")";
}

std::string SqlQueries::createInvertersTable() {
    return "CREATE Table Inverters ("
            "Serial INT UNSIGNED NOT NULL,"
            "Name varchar(32),"
            "Type varchar(32),"
            "SW_Version varchar(32),"
            "TimeStamp datetime,"
            "TotalPac int,"
            "EToday int(8),"
            "ETotal int(8),"
            "OperatingTime double,"
            "FeedInTime double,"
            "Status varchar(10),"
            "GridRelay varchar(10),"
            "Temperature float,"
            "PRIMARY KEY (Serial)"
    ")";
}

std::list<std::string> SqlQueries::exportLiveData(const LiveData& liveData) {
    std::list<std::string> out;

    /*
    {
        std::stringstream sql;
        sql << "INSERT INTO SpotData VALUES (" <<
            liveData.timestamp << ',' <<
            liveData.serial << ',' <<
            liveData.dc.at(0).power << ',' <<
            liveData.dc.at(1).power << ',' <<
            liveData.dc.at(0).current << ',' <<
            liveData.dc.at(1).current << ',' <<
            liveData.dc.at(0).voltage << ',' <<
            liveData.dc.at(1).voltage << ',' <<
            liveData.ac.at(0).power << ',' <<
            liveData.ac.at(1).power << ',' <<
            liveData.ac.at(2).power << ',' <<
            liveData.ac.at(0).current << ',' <<
            liveData.ac.at(1).current << ',' <<
            liveData.ac.at(2).current << ',' <<
            liveData.ac.at(0).voltage << ',' <<
            liveData.ac.at(1).voltage << ',' <<
            liveData.ac.at(2).voltage << ',' <<
            liveData.energyExportToday << ',' <<
            liveData.energyExportTotal << ',' <<
            // TODO: fix these values
            0.0f << "," << //(float)liveData.GridFreq/100 << ',' <<
            0.0 << "," << //(double)liveData.OperationTime/3600 << ',' <<
            0.0 << "," << //(double)liveData.FeedInTime/3600 << ',' <<
            0.0f << "," << //(float)liveData.BT_Signal << ',' <<
            "''" << "," << //s_quoted(status_text(liveData.DeviceStatus)) << ',' <<
            "''" << "," << //s_quoted(status_text(liveData.GridRelayStatus)) << ',' <<
            0.0f << ")"; //(float)liveData.Temperature/100 << ")";

        out.push_back(sql.str());
    }
*/

    {
        uint16_t cls = 1;
        for (const auto& dc : liveData.dc) {
            std::stringstream sql;
            sql << "INSERT INTO LiveDataDc VALUES (" <<
                liveData.timestamp << ',' <<
                liveData.serial << ',' <<
                cls++ << ',' <<
                dc.power << ',' <<
                dc.current << ',' <<
                dc.voltage << ")";
            out.push_back(sql.str());
        }
    }

    {
        std::stringstream sql;
        sql << "INSERT INTO LiveDataAc VALUES ("
            << liveData.timestamp
            << ',' << liveData.serial;
        for (const auto& ac : liveData.ac) {
            sql << ',' << ac.power
                << ',' << ac.current
                << ',' << ac.voltage;
        }
        sql << "," << liveData.energyExportToday
            << "," << liveData.energyExportTotal
            << "," << liveData.energyImportTotal;
        sql << ")";
        out.push_back(sql.str());
    }

    return out;
}

std::string SqlQueries::exportSpotData(std::time_t timestamp, const InverterData& inv) {
    std::stringstream sql;
    sql << "INSERT INTO SpotData VALUES(" <<
           timestamp << ',' <<
           inv.serial << ',' <<
           inv.Pdc1 << ',' <<
           inv.Pdc2 << ',' <<
           (float)inv.Idc1/1000 << ',' <<
           (float)inv.Idc2/1000 << ',' <<
           (float)inv.Udc1/100 << ',' <<
           (float)inv.Udc2/100 << ',' <<
           inv.Pac1 << ',' <<
           inv.Pac2 << ',' <<
           inv.Pac3 << ',' <<
           (float)inv.Iac1/1000 << ',' <<
           (float)inv.Iac2/1000 << ',' <<
           (float)inv.Iac3/1000 << ',' <<
           (float)inv.Uac1/100 << ',' <<
           (float)inv.Uac2/100 << ',' <<
           (float)inv.Uac3/100 << ',' <<
           inv.EToday << ',' <<
           inv.ETotal << ',' <<
           (float)inv.GridFreq/100 << ',' <<
           (double)inv.OperationTime/3600 << ',' <<
           (double)inv.FeedInTime/3600 << ',' <<
           (float)inv.BT_Signal << ',' <<
           s_quoted(status_text(inv.DeviceStatus)) << ',' <<
           s_quoted(status_text(inv.GridRelayStatus)) << ',' <<
           (float)inv.Temperature/100 << ")";

    return sql.str();
}

SqlQueries::SqlQueries() {
}

std::string SqlQueries::status_text(int status) {
    switch (status)
    {
    //Grid Relay Status
    case 311: return "Open";
    case 51: return "Closed";

        //Device Status
    case 307: return "OK";
    case 455: return "Warning";
    case 35: return "Fault";

        //NaNStt=Information not available
    case 0xFFFFFD: return "N/A";

    default: return "?";
    }
}

} // namespace sql
