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

#if defined(USE_SQLITE)

#include "db_SQLite_Export.h"

#include <Config.h>
#include <LiveData.h>
#include <Logger.h>
#include <TagDefs.h>
#include <sql/SqlQueries.h>

db_SQL_Export::db_SQL_Export(const SqlConfig& config) :
    db_SQL_Base(config),
    m_config(config)
{
}

bool db_SQL_Export::open()
{
    auto rc = db_SQL_Base::open(m_config.databaseName);
    LOG_IF_F(ERROR, rc, "Error opening the database %s", m_config.databaseName.c_str());
    return rc;
}

void db_SQL_Export::close()
{
    db_SQL_Base::close();
}

void db_SQL_Export::exportLiveData(const LiveData& liveData)
{
    LOG_IF_F(WARNING, !isopen(), "Database is not open");
    if (!isopen())
        return;

    auto queries = sql::SqlQueries::exportLiveData(liveData);
    for (const auto& query : queries) {
        LOG_IF_F(ERROR, exec_query(query), "exec_query() returned %s", query.c_str());
    }
}

void db_SQL_Export::exportDayData(const std::vector<InverterData>& inverters)
{
    if (!isopen()) return;

    const char *sql = "INSERT INTO DayData(TimeStamp,Serial,TotalYield,Power,PVoutput) VALUES(?1,?2,?3,?4,?5)";
    int rc = SQLITE_OK;

    sqlite3_stmt* pStmt;
    if ((rc = sqlite3_prepare_v2(m_dbHandle, sql, strlen(sql), &pStmt, NULL)) == SQLITE_OK)
    {
        exec_query("BEGIN IMMEDIATE TRANSACTION");

        for (const auto& inverter : inverters)
        {
            const unsigned int numelements = sizeof(inverter.dayData)/sizeof(DayData);
            unsigned int first_rec, last_rec;
            // Find first record with production data
            for (first_rec = 0; first_rec < numelements; first_rec++)
            {
                if ((inverter.dayData[first_rec].datetime == 0) || (inverter.dayData[first_rec].watt != 0))
                {
                    // Include last zero record, just before production starts
                    if (first_rec > 0) first_rec--;
                    break;
                }
            }

            // Find last record with production data
            for (last_rec = numelements-1; last_rec > first_rec; last_rec--)
            {
                if ((inverter.dayData[last_rec].datetime != 0) && (inverter.dayData[last_rec].watt != 0))
                    break;
            }

            // Include zero record, just after production stopped
            if ((last_rec < numelements - 1) && (inverter.dayData[last_rec + 1].datetime != 0))
                last_rec++;

            if (first_rec < last_rec) // Production data found or all zero?
            {
                // Store data from first to last record
                for (unsigned int idx = first_rec; idx <= last_rec; idx++)
                {
                    // Invalid dates are not written to db
                    if (inverter.dayData[idx].datetime > 0)
                    {
                        sqlite3_bind_int(pStmt, 1, inverter.dayData[idx].datetime);
                        // Fix #269
                        // To store unsigned int32 serial numbers, we're using sqlite3_bind_int64
                        // SQLite will store these uint32 in 4 bytes
                        sqlite3_bind_int64(pStmt, 2, inverter.serial);
                        sqlite3_bind_int64(pStmt, 3, inverter.dayData[idx].totalWh);
                        sqlite3_bind_int64(pStmt, 4, inverter.dayData[idx].watt);
                        sqlite3_bind_null(pStmt, 5);

                        rc = sqlite3_step(pStmt);
                        if ((rc != SQLITE_DONE) && (rc != SQLITE_CONSTRAINT))
                        {
                            LOG_F(ERROR, "[day_data]sqlite3_step() returned");
                            break;
                        }

                        sqlite3_clear_bindings(pStmt);
                        sqlite3_reset(pStmt);
                        rc = SQLITE_OK;
                    }
                }
            }
        }

        sqlite3_finalize(pStmt);

        if (rc == SQLITE_OK)
            exec_query("COMMIT");
        else
        {
            LOG_F(ERROR, "[day_data]Transaction failed. Rolling back now...");
            exec_query("ROLLBACK");
        }
    }
}

void db_SQL_Export::exportMonthData(const std::vector<InverterData>& inverters)
{
    if (!isopen()) return;

    const char *sql = "INSERT INTO MonthData(TimeStamp,Serial,TotalYield,DayYield) VALUES(?1,?2,?3,?4)";
    int rc = SQLITE_OK;

    sqlite3_stmt* pStmt;
    if ((rc = sqlite3_prepare_v2(m_dbHandle, sql, strlen(sql), &pStmt, NULL)) == SQLITE_OK)
    {
        exec_query("BEGIN IMMEDIATE TRANSACTION");

        for (const auto& inverter : inverters)
        {
            //Fix Issue 74: Double data in Monthdata tables
            tm *ptm = gmtime(&inverter.monthData[0].datetime);
            char dt[32];
            strftime(dt, sizeof(dt), "%Y-%m", ptm);

            std::stringstream rmvsql;
            rmvsql.str("");
            rmvsql << "DELETE FROM MonthData WHERE Serial=" << inverter.serial << " AND strftime('%Y-%m',datetime(TimeStamp, 'unixepoch'))='" << dt << "';";

            rc = exec_query(rmvsql.str().c_str());
            if (rc != SQLITE_OK)
            {
                LOG_F(ERROR, "[month_data]sqlite3_exec() returned");
                break;
            }

            for (unsigned int idx = 0; idx < sizeof(inverter.monthData)/sizeof(MonthData); idx++)
            {
                if (inverter.monthData[idx].datetime > 0)
                {
                    sqlite3_bind_int(pStmt, 1, inverter.monthData[idx].datetime);
                    // Fix #269
                    // To store unsigned int32 serial numbers, we're using sqlite3_bind_int64
                    // SQLite will store these uint32 in 4 bytes
                    sqlite3_bind_int64(pStmt, 2, inverter.serial);
                    sqlite3_bind_int64(pStmt, 3, inverter.monthData[idx].totalWh);
                    sqlite3_bind_int64(pStmt, 4, inverter.monthData[idx].dayWh);

                    rc = sqlite3_step(pStmt);
                    if ((rc != SQLITE_DONE) && (rc != SQLITE_CONSTRAINT))
                    {
                        LOG_F(ERROR, "[month_data]sqlite3_step() returned");
                        break;
                    }

                    sqlite3_clear_bindings(pStmt);
                    sqlite3_reset(pStmt);
                    rc = SQLITE_OK;
                }
            }
        }

        sqlite3_finalize(pStmt);

        if (rc == SQLITE_OK)
            exec_query("COMMIT");
        else
        {
            LOG_F(ERROR, "[month_data]Transaction failed. Rolling back now...");
            exec_query("ROLLBACK");
        }
    }
}

void db_SQL_Export::exportSpotData(std::time_t timestamp, const std::vector<InverterData>& data)
{
    LOG_IF_F(WARNING, !isopen(), "Database is not open");
    if (!isopen()) return;

    type_label(data);
    device_status(data, timestamp);

    std::stringstream sql;
    for (const auto& inv : data)
    {
        auto str = sql::SqlQueries::exportSpotData(timestamp, inv);
        if ((exec_query(str)) != SQLITE_OK)
        {
            LOG_F(ERROR, "exec_query() returned %s", sql.str().c_str());
            break;
        }
    }
}

void db_SQL_Export::exportEventData(const std::vector<InverterData>& inverters, const std::string& dt_range_csv)
{
    if (!isopen()) return;

    const char *sql = "INSERT INTO EventData(EntryID,TimeStamp,Serial,SusyID,EventCode,EventType,Category,EventGroup,Tag,OldValue,NewValue,UserGroup) VALUES(?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12)";
    int rc = SQLITE_OK;

    sqlite3_stmt* pStmt;
    if ((rc = sqlite3_prepare_v2(m_dbHandle, sql, strlen(sql), &pStmt, NULL)) == SQLITE_OK)
    {
        exec_query("BEGIN IMMEDIATE TRANSACTION");

        for (const auto& inverter : inverters)
        {
            for (auto it=inverter.eventData.begin(); it!=inverter.eventData.end(); ++it)
            {
                std::string grp = tagdefs.getDesc(it->Group());
                std::string tag = tagdefs.getDesc(it->Tag());

                // If description contains "%s", replace it with localized parameter
                size_t start_pos = tag.find("%s");
                if (start_pos != std::string::npos)
                    tag.replace(start_pos, 2, tagdefs.getDescForLRI(it->Parameter()));

                std::string usrgrp = tagdefs.getDesc(it->UserGroupTagID());
                std::stringstream oldval;
                std::stringstream newval;

                switch (it->DataType())
                {
                case DT_STATUS:
                    oldval << tagdefs.getDesc(it->OldVal() & 0xFFFF);
                    newval << tagdefs.getDesc(it->NewVal() & 0xFFFF);
                    break;

                case DT_STRING:
                    oldval.width(8); oldval.fill('0');
                    oldval << it->OldVal();
                    newval.width(8); newval.fill('0');
                    newval << it->NewVal();
                    break;

                default:
                    oldval << it->OldVal();
                    newval << it->NewVal();
                }

                sqlite3_bind_int(pStmt,  1, it->EntryID());
                sqlite3_bind_int(pStmt,  2, it->DateTime());
                // Fix #269
                // To store unsigned int32 serial numbers, we're using sqlite3_bind_int64
                // SQLite will store these uint32 in 4 bytes
                sqlite3_bind_int64(pStmt,  3, it->SerNo());
                sqlite3_bind_int(pStmt,  4, it->SUSyID());
                sqlite3_bind_int(pStmt,  5, it->EventCode());
                sqlite3_bind_text(pStmt, 6, it->EventType().c_str(), it->EventType().size(), SQLITE_TRANSIENT);
                sqlite3_bind_text(pStmt, 7, it->EventCategory().c_str(), it->EventCategory().size(), SQLITE_TRANSIENT);
                sqlite3_bind_text(pStmt, 8, grp.c_str(), grp.size(), SQLITE_TRANSIENT);
                sqlite3_bind_text(pStmt, 9, tag.c_str(), tag.size(), SQLITE_TRANSIENT);
                sqlite3_bind_text(pStmt,10, oldval.str().c_str(), oldval.str().size(), SQLITE_TRANSIENT);
                sqlite3_bind_text(pStmt,11, newval.str().c_str(), newval.str().size(), SQLITE_TRANSIENT);
                sqlite3_bind_text(pStmt,12, usrgrp.c_str(), usrgrp.size(), SQLITE_TRANSIENT);

                rc = sqlite3_step(pStmt);
                if ((rc != SQLITE_DONE) && (rc != SQLITE_CONSTRAINT))
                {
                    LOG_F(ERROR, "[event_data]sqlite3_step() returned");
                    break;
                }

                sqlite3_clear_bindings(pStmt);
                sqlite3_reset(pStmt);
                rc = SQLITE_OK;
            } //for
        }

        sqlite3_finalize(pStmt);

        if (rc == SQLITE_OK)
            rc = exec_query("COMMIT");
        else
        {
            LOG_F(ERROR, "[event_data]Transaction failed. Rolling back now...");
            rc = exec_query("ROLLBACK");
        }
    }
}

void db_SQL_Export::exportBatteryData(std::time_t timestamp, const std::vector<InverterData>& inverters)
{
    if (!isopen()) return;

    const char *sql = "INSERT INTO SpotDataX(TimeStamp,Serial,Key,Value) VALUES(?1,?2,?3,?4)";
    int rc = SQLITE_OK;

    sqlite3_stmt* pStmt;
    if ((rc = sqlite3_prepare_v2(m_dbHandle, sql, strlen(sql), &pStmt, NULL)) == SQLITE_OK)
    {
        exec_query("BEGIN IMMEDIATE TRANSACTION");

        for (const auto& inverter : inverters)
        {
            if ((inverter.DevClass == BatteryInverter) || (inverter.hasBattery))
            {
                if ((rc = insert_battery_data(pStmt, timestamp, inverter.serial, BatChaStt >> 8, inverter.BatChaStt)) != SQLITE_OK) break;
                if ((rc = insert_battery_data(pStmt, timestamp, inverter.serial, BatTmpVal >> 8, inverter.BatTmpVal)) != SQLITE_OK) break;
                if ((rc = insert_battery_data(pStmt, timestamp, inverter.serial, BatVol >> 8, inverter.BatVol)) != SQLITE_OK) break;
                if ((rc = insert_battery_data(pStmt, timestamp, inverter.serial, BatAmp >> 8, inverter.BatAmp)) != SQLITE_OK) break;
                //if ((rc = insert_battery_data(pStmt, timestamp, inverter.serial, BatDiagCapacThrpCnt >> 8, inverter.BatDiagCapacThrpCnt)) != SQLITE_OK) break;
                //if ((rc = insert_battery_data(pStmt, timestamp, inverter.serial, BatDiagTotAhIn >> 8, inverter.BatDiagTotAhIn)) != SQLITE_OK) break;
                //if ((rc = insert_battery_data(pStmt, timestamp, inverter.serial, BatDiagTotAhOut >> 8, inverter.BatDiagTotAhOut)) != SQLITE_OK) break;
                if ((rc = insert_battery_data(pStmt, timestamp, inverter.serial, MeteringGridMsTotWIn >> 8, inverter.MeteringGridMsTotWIn)) != SQLITE_OK) break;
                if ((rc = insert_battery_data(pStmt, timestamp, inverter.serial, MeteringGridMsTotWOut >> 8, inverter.MeteringGridMsTotWOut)) != SQLITE_OK) break;
            }
        }

        sqlite3_finalize(pStmt);

        if (rc == SQLITE_OK)
            exec_query("COMMIT");
        else
        {
            LOG_F(ERROR, "[battery_data]Transaction failed. Rolling back now...");
            exec_query("ROLLBACK");
        }
    }
}

int db_SQL_Export::insert_battery_data(sqlite3_stmt* pStmt, int32_t tm, int32_t sn, int32_t key, int32_t val)
{
    int rc = SQLITE_OK;

    sqlite3_bind_int(pStmt, 1, tm);
    sqlite3_bind_int(pStmt, 2, sn);
    sqlite3_bind_int(pStmt, 3, key);
    sqlite3_bind_int(pStmt, 4, val);

    rc = sqlite3_step(pStmt);

    if (rc != SQLITE_DONE)
        LOG_F(ERROR, "[battery_data]sqlite3_step() returned");
    else
        rc = SQLITE_OK;

    sqlite3_clear_bindings(pStmt);
    sqlite3_reset(pStmt);

    return rc;
}

#endif
