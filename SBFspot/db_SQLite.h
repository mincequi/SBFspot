/************************************************************************************************
	SBFspot - Yet another tool to read power production of SMA solar inverters
	(c)2012-2018, SBF

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

#if defined(USE_SQLITE)

#include "osselect.h"
#include "Types.h"
#include <ctime>
#include <iostream>
#include <sqlite3.h>

extern int quiet;
extern int verbose;

#define SQL_MINIMUM_SCHEMA_VERSION 1
#define SQL_RECOMMENDED_SCHEMA_VERSION 1
#define SQL_BUSY_RETRY_COUNT 20

struct SqlConfig;

class db_SQL_Base
{
public:
	enum
	{
		SQL_OK = SQLITE_OK,
		SQL_ERROR = SQLITE_ERROR
	};

protected:
    const SqlConfig& m_config;
    sqlite3 *m_dbHandle = nullptr;
	std::string m_database;

public:
    db_SQL_Base(const SqlConfig& config);
	~db_SQL_Base() { if (m_dbHandle) close(); }
    int open(const std::string& database);
	int close(void);
	int exec_query(std::string qry);
	std::string errortext(void) { return m_dbHandle ? sqlite3_errmsg(m_dbHandle) : "Unable to open the database file [" + m_database + "]"; }
	bool isopen(void) { return (m_dbHandle != NULL); }
    int type_label(const std::vector<InverterData>& inverters);
    int device_status(const std::vector<InverterData>& inverters, time_t spottime);
	int batch_get_archdaydata(std::string &data, unsigned int Serial, int datelimit, int statuslimit, int& recordcount);
	int batch_set_pvoflag(const std::string &data, unsigned int Serial);
	int set_config(const std::string key, const std::string value);
	int get_config(const std::string key, std::string &value);
	int get_config(const std::string key, int &value);
    // Obtain InverterData set for given time span
    DataPerInverter getInverterData(std::time_t startTime, std::time_t endTime);
};

#endif //#if defined(USE_SQLITE)
