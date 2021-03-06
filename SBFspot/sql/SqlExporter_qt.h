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

#include <QObject>
#include <QSqlDatabase>

#include <Exporter.h>
#include <Storage.h>

struct SqlConfig;

namespace sql {

class SqlExporter_qt : public Exporter, public Storage {
public:
    SqlExporter_qt(const SqlConfig& config);

    bool init() override;
    bool open() override;
    void close() override;

    void exportLiveData(const LiveData& liveData) override;
    void exportDayData(const std::vector<DayData>& dayData) override;
    void exportMonthData(const std::vector<MonthData>& monthData) override;

    Storage::MissingSequence nextMissingDayData(std::time_t now, const Serial& serial) override;
    Storage::MissingSequence nextMissingMonthData(std::time_t now, const Serial& serial) override;
    void setEndOfDayData(std::time_t timestamp, const Serial& serial) override;
    void setEndOfMonthData(std::time_t timestamp, const Serial& serial) override;

private:
    bool createTables();

    const SqlConfig& m_config;

    QSqlDatabase m_db;
    std::map<uint32_t, std::time_t> m_endOfDayData;
    std::map<uint32_t, std::time_t> m_endOfMonthData;
};

} // namespace sql
