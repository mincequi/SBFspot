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

#include "db_SQLite.h"

#include <sys/time.h>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <Config.h>
#include <Logger.h>
#include <sql/SqlQueries.h>

using namespace std;

db_SQL_Base::db_SQL_Base(const SqlConfig& config) :
    m_config(config)
{
}

int db_SQL_Base::open(const string& database)
{
	int result = SQLITE_OK;

	if (sqlite3_threadsafe() == 0)
	{
        LOG_S(ERROR) << "SQLite3 libs are not threadsafe";
		return SQLITE_ERROR;
	}

	if (!m_dbHandle)	// Not yet open?
	{
        m_database = m_config.databaseName;

		if (database.size() > 0)
			result = sqlite3_open_v2(database.c_str(), &m_dbHandle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_FULLMUTEX, NULL);
		else
			result = SQLITE_ERROR;

    	if(result == SQLITE_OK)
			sqlite3_busy_timeout(m_dbHandle, 2000);
		else
		{
            LOG_S(ERROR) << "Can't open SQLite db [" << m_database << "]";
			m_dbHandle = NULL;
		}
	}

	return result;
}


int db_SQL_Base::close(void)
{
	int result = SQLITE_OK;

	if((result = sqlite3_close(m_dbHandle)) != SQLITE_OK)
        LOG_S(ERROR) << "Can't close SQLite db [" << m_database << "]";
    else
	{
		m_database = "";
		m_dbHandle = NULL;
	}

	return result;
}

int db_SQL_Base::exec_query(string qry)
{
	int result = SQLITE_OK;
	int retrycount = 0;

	do
	{
		result = sqlite3_exec(m_dbHandle, qry.c_str(), NULL, NULL, NULL);
		if (result != SQLITE_OK)
		{
            LOG_S(ERROR) << "sqlite3_exec() returned" << qry;
			if (++retrycount > SQL_BUSY_RETRY_COUNT)
				break;
		}

	} while ((result == SQLITE_BUSY) || (result == SQLITE_LOCKED));

	if ((result == SQLITE_OK) && (retrycount > 0))
	{
		std::cout << "Query executed successfully after " << retrycount << " time" << (retrycount == 1 ? "" : "s") << std::endl;
	}

	return result;
}

int db_SQL_Base::type_label(const std::vector<InverterData>& inverters)
{
	std::stringstream sql;
	int rc = SQLITE_OK;

    for (const auto& inverter : inverters)
	{
		sql.str("");

		// Instead of using REPLACE which is actually a DELETE followed by INSERT,
		// we do an INSERT OR IGNORE (for new records) followed by UPDATE (for existing records)
		sql << "INSERT OR IGNORE INTO Inverters VALUES(" <<
            inverter.Serial << ',' <<
            sql::SqlQueries::s_quoted(inverter.DeviceName) << ',' <<
            sql::SqlQueries::s_quoted(inverter.DeviceType) << ',' <<
            sql::SqlQueries::s_quoted(inverter.SWVersion) << ',' <<
			"0,0,0,0,0,0,'','',0)";

		if ((rc = exec_query(sql.str())) != SQLITE_OK)
            LOG_S(ERROR) << "exec_query() returned" << sql.str();

		sql.str("");

		sql << "UPDATE Inverters SET" <<
            " Name=" << sql::SqlQueries::s_quoted(inverter.DeviceName) <<
            ",Type=" << sql::SqlQueries::s_quoted(inverter.DeviceType) <<
            ",SW_Version=" << sql::SqlQueries::s_quoted(inverter.SWVersion) <<
            " WHERE Serial=" << inverter.Serial;

		if ((rc = exec_query(sql.str())) != SQLITE_OK)
            LOG_S(ERROR) << "exec_query() returned" << sql.str();
	}

	return rc;
}

int db_SQL_Base::device_status(const std::vector<InverterData>& inverters, time_t spottime)
{
	std::stringstream sql;
	int rc = SQLITE_OK;

	// Take time from computer instead of inverter
	//time_t spottime = cfg->SpotTimeSource == 0 ? inverters[0]->InverterDatetime : time(NULL);

    for (const auto& inverter : inverters)
	{
		sql.str("");

		sql << "UPDATE Inverters SET" <<
            " TimeStamp=" << std::to_string(spottime) <<
            ",TotalPac=" << inverter.TotalPac <<
            ",EToday=" << inverter.EToday <<
            ",ETotal=" << inverter.ETotal <<
            ",OperatingTime=" << (double)inverter.OperationTime/3600 <<
            ",FeedInTime=" << (double)inverter.FeedInTime/3600 <<
            ",Status=" << sql::SqlQueries::s_quoted(sql::SqlQueries::status_text(inverter.DeviceStatus)) <<
            ",GridRelay=" << sql::SqlQueries::s_quoted(sql::SqlQueries::status_text(inverter.GridRelayStatus)) <<
            ",Temperature=" << (float)inverter.Temperature/100 <<
            " WHERE Serial=" << inverter.Serial;

		if ((rc = exec_query(sql.str())) != SQLITE_OK)
            LOG_S(ERROR) << "exec_query() returned" << sql.str();
	}

	return rc;
}

int db_SQL_Base::batch_get_archdaydata(std::string &data, unsigned int Serial, int datelimit, int statuslimit, int& recordcount)
{
	std::stringstream sql;
	int rc = SQLITE_OK;
	recordcount = 0;

	sqlite3_stmt *pStmt = NULL;

	sql << "SELECT strftime('%Y%m%d,%H:%M',TimeStamp),V1,V2,V3,V4,V5,V6,V7,V8,V9,V10,V11,V12 FROM [vwPvoData] WHERE "
        "TimeStamp>DATE(DATE(),'" << -(datelimit-2) << " day') "
        "AND PVoutput IS NULL "
        "AND Serial=" << Serial << " "
        "ORDER BY TimeStamp "
        "LIMIT " << statuslimit;

	rc = sqlite3_prepare_v2(m_dbHandle, sql.str().c_str(), -1, &pStmt, NULL);

	if (pStmt != NULL)
	{
		std::stringstream result;
		while (sqlite3_step(pStmt) == SQLITE_ROW)
		{
			result.str("");

			string dt = (char *)sqlite3_column_text(pStmt, 0);
			// Energy Generation
			int64_t V1 = sqlite3_column_int64(pStmt, 1);
			// Power Generation
			int64_t V2 = sqlite3_column_int64(pStmt, 2);

			// from 2nd record, add a record separator
			if (!data.empty()) result << ";";

			// Mandatory values
			result << dt << "," << V1 << "," << V2;

			result << ",";
			// Energy Consumption
			if (sqlite3_column_type(pStmt, 3) != SQLITE_NULL)
				result << sqlite3_column_int64(pStmt, 3);

			result << ",";
			// Power Consumption
			if (sqlite3_column_type(pStmt, 4) != SQLITE_NULL)
				result << sqlite3_column_int64(pStmt, 4);

			result << ",";
			// Temperature
			if (sqlite3_column_type(pStmt, 5) != SQLITE_NULL)
				result << sqlite3_column_double(pStmt, 5);

			result << ",";
			// Voltage
			if (sqlite3_column_type(pStmt, 6) != SQLITE_NULL)
				result << sqlite3_column_double(pStmt, 6);

			// Extended values
			for (int extval = 7; extval <= 12; extval++)
			{
				result << ",";
				if (sqlite3_column_type(pStmt, extval) != SQLITE_NULL)
					result << sqlite3_column_double(pStmt, extval);
			}

			const std::string& str = result.str();
			int end = str.length();

			for (std::string::const_reverse_iterator it=str.rbegin(); it!=str.rend(); ++it, end--)
			{
				if ((*it) != ',')
					break;
			}

			data.append(result.str().substr(0, end));
			recordcount++;
		}

		sqlite3_finalize(pStmt);
	}

	return rc;
}

int db_SQL_Base::batch_set_pvoflag(const std::string &data, unsigned int Serial)
{
	std::stringstream sql;
	int rc = SQLITE_OK;

	vector<std::string> items;
	boost::split(items, data, boost::is_any_of(";"));

	sql << "UPDATE OR ROLLBACK DayData "
		"SET PVoutput=1 "
		"WHERE Serial=" << Serial << " "
		"AND strftime('%Y%m%d,%H:%M',datetime(TimeStamp, 'unixepoch', 'localtime')) "
		"IN (";

	bool firstitem = true;
	for (vector<std::string>::iterator it=items.begin(); it!=items.end(); ++it)
	{
		if (it->substr(15, 1) == "1")
		{
			if (!firstitem)
				sql << ",";
			else
				firstitem = false;
            sql << sql::SqlQueries::s_quoted(it->substr(0, 14));
		}
	}

	sql << ")";

	if ((rc = exec_query(sql.str())) != SQLITE_OK)
        LOG_S(ERROR) << "exec_query() returned" << sql.str();

	return rc;
}

int db_SQL_Base::set_config(const std::string key, const std::string value)
{
	std::stringstream sql;
	int rc = SQLITE_OK;

	sql << "INSERT OR REPLACE INTO Config (`Key`,`Value`) VALUES('" << key << "','" << value << "')";

	if ((rc = exec_query(sql.str())) != SQLITE_OK)
        LOG_S(ERROR) << "exec_query() returned" << sql.str();

	return rc;
}

int db_SQL_Base::get_config(const std::string key, std::string &value)
{
	std::stringstream sql;
	int rc = SQLITE_OK;

	sqlite3_stmt *pStmt = NULL;

	sql << "SELECT `Value` FROM Config WHERE `Key`='" << key << "'";

	rc = sqlite3_prepare_v2(m_dbHandle, sql.str().c_str(), -1, &pStmt, NULL);

	if (pStmt != NULL)
	{
		while (sqlite3_step(pStmt) == SQLITE_ROW)
		{
			value = (char *)sqlite3_column_text(pStmt, 0);
		}
		sqlite3_finalize(pStmt);
	}

	return rc;
}

int db_SQL_Base::get_config(const std::string key, int &value)
{
	int rc = SQLITE_OK;
    std::string strValue = std::to_string(value);
	if((rc = get_config(key, strValue)) == SQL_OK)
		value = boost::lexical_cast<int>(strValue);

	return rc;
}

DataPerInverter db_SQL_Base::getInverterData(std::time_t startTime, std::time_t endTime)
{
    std::stringstream sql;
    sql << "SELECT TimeStamp,Serial,Pdc1,Pdc2,Pac1,Pac2,Pac3 FROM SpotData WHERE"
        " TimeStamp >= " << startTime <<
        " AND TimeStamp <= " << endTime;

    sqlite3_stmt *pStmt = nullptr;
    auto rc = sqlite3_prepare_v2(m_dbHandle, sql.str().c_str(), -1, &pStmt, NULL);
    if (pStmt == nullptr || rc != SQLITE_OK)
    {
        std::cerr << "SQL error: " << rc;
        return {};
    }

    DataPerInverter out;
    while (sqlite3_step(pStmt) == SQLITE_ROW)
    {
        InverterData data;
        data.InverterDatetime = sqlite3_column_int64(pStmt, 0);
        data.Serial = sqlite3_column_int64(pStmt, 1);
        data.Pdc1 = sqlite3_column_int(pStmt, 2);
        data.Pdc2 = sqlite3_column_int(pStmt, 3);
        data.Pac1 = sqlite3_column_int(pStmt, 4);
        data.Pac2 = sqlite3_column_int(pStmt, 5);
        data.Pac3 = sqlite3_column_int(pStmt, 6);
        data.TotalPac = data.Pac1 + data.Pac2 + data.Pac3;
        out[data.Serial].push_back(data);
    }

    sqlite3_finalize(pStmt);

    return out;
}

#endif // #if defined(USE_SQLITE)
