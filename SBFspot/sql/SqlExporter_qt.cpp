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

#include "SqlExporter_qt.h"

#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVariant>

#include <Config.h>
#include <LiveData.h>
#include <Logger.h>
#include <sql/SqlQueries.h>

namespace sql {

/*
enum class SqlTables {
    LiveDataAc,
    LiveDataDc,
    LiveDataAux,

    HistoricDayData,
    HistoricMonthData
};
Q_ENUM_NS(SqlTables)
*/

SqlExporter_qt::SqlExporter_qt(const SqlConfig& config) :
    m_config(config) {
    switch (m_config.type) {
    case SqlType::SqLite:
        m_db = QSqlDatabase::addDatabase("QSQLITE", "QSQLITE");
        break;
    case SqlType::MySql:
        m_db = QSqlDatabase::addDatabase("QMYSQL", "QMYSQL");
        break;
    default:
        LOG_F(ERROR, "Unknown database type %i", m_config.type);
        return;
    }

    auto dbName = QString::fromStdString(m_config.databaseName);
    if (dbName.startsWith("~/")) dbName.replace(0, 1, QDir::homePath());
    m_db.setDatabaseName(dbName);
    m_db.setHostName(QString::fromStdString(m_config.hostName));
    m_db.setPort(m_config.port);
    m_db.setUserName(QString::fromStdString(m_config.userName));
    m_db.setPassword(QString::fromStdString(m_config.password));

    LOG_IF_S(FATAL, !m_db.open()) << m_db.lastError().text().toStdString() << ": " << m_db.databaseName().toStdString();
    m_db.close();
}

bool SqlExporter_qt::init() {
    return createTables();
}

bool SqlExporter_qt::open() {
    return m_db.open();
}

void SqlExporter_qt::close() {
    return m_db.close();
}

void SqlExporter_qt::exportLiveData(const LiveData& liveData) {
    LOG_IF_F(WARNING, !m_db.isOpen(), "Database is not open");
    if (!m_db.isOpen())
        return;

    auto queries = sql::SqlQueries::exportLiveData(liveData);
    for (const auto& query : queries) {
        m_db.exec(QString::fromStdString(query));
        if (m_db.lastError().type() != QSqlError::NoError) {
            LOG_S(WARNING) << "Error inserting data: " << m_db.lastError().text().toStdString();
        }
    }
}

void SqlExporter_qt::exportDayData(const std::vector<DayData>& dayData) {
    LOG_IF_F(WARNING, !m_db.isOpen(), "Database is not open");
    if (!m_db.isOpen())
        return;

    QSqlQuery q("insert into DayData (TimeStamp, Serial, TotalYield) "
                "VALUES (?, ?, ?)", m_db);

    QVariantList timestamps;
    QVariantList serials;
    QVariantList yields;
    //QVariantList powers;
    //QVariantList pvOutputs;

    for (const auto& dd : dayData) {
        timestamps << static_cast<uint32_t>(dd.datetime);
        serials << dd.serial.serial();
        yields << dd.totalWh;
        //powers << QVariant();
        //pvOutputs << QVariant();
    }

    q.addBindValue(timestamps);
    q.addBindValue(serials);
    q.addBindValue(yields);
    //q.addBindValue(powers);
    //q.addBindValue(pvOutputs);

    LOG_S(INFO) << "Inserting day data for: " << dayData.back().serial << ", from: " << dayData.front().datetime << ", to: " << dayData.back().datetime;
    if (!q.execBatch()) {
        LOG_S(ERROR) << q.lastError().text().toStdString();
    }
}

void SqlExporter_qt::exportMonthData(const std::vector<MonthData>& monthData) {
    LOG_IF_F(WARNING, !m_db.isOpen(), "Database is not open");
    if (!m_db.isOpen())
        return;

    QSqlQuery q("insert into MonthData (TimeStamp, Serial, TotalYield) "
                "VALUES (?, ?, ?)", m_db);

    QVariantList timestamps;
    QVariantList serials;
    QVariantList yields;

    for (const auto& dd : monthData) {
        timestamps << static_cast<uint32_t>(dd.datetime);
        serials << dd.serial.serial();
        yields << dd.totalWh;
    }

    q.addBindValue(timestamps);
    q.addBindValue(serials);
    q.addBindValue(yields);

    LOG_S(INFO) << "Inserting month data for: " << monthData.back().serial << ", from: " << monthData.front().datetime << ", to: " << monthData.back().datetime;
    if (!q.execBatch()) {
        LOG_S(ERROR) << q.lastError().text().toStdString();
    }
}

bool SqlExporter_qt::createTables() {
    if (!m_db.open()) {
        LOG_S(WARNING) << m_db.lastError().text().toStdString();
        return false;
    }

    auto tables = m_db.tables();
    LOG_S(1) << "Database: " << m_config.databaseName << ", tables: " << tables.join(", ").toStdString();

    if (!tables.contains("LiveDataDc")) {
        m_db.exec(QString::fromStdString(SqlQueries::createTableLiveDataDc()));
        if (m_db.lastError().type() != QSqlError::NoError) {
            LOG_S(ERROR) << "Error creating table: " << m_db.lastError().text().toStdString();
            m_db.close();
            return false;
        }
    }

    if (!tables.contains("LiveDataAc")) {
        m_db.exec(QString::fromStdString(SqlQueries::createTableLiveDataAc()));
        if (m_db.lastError().type() != QSqlError::NoError) {
            LOG_S(ERROR) << "Error creating table: " << m_db.lastError().text().toStdString();
            m_db.close();
            return false;
        }
    }

    if (!tables.contains("LiveDataAux")) {
        m_db.exec(QString::fromStdString(SqlQueries::createTableLiveDataAux()));
        if (m_db.lastError().type() != QSqlError::NoError) {
            LOG_S(ERROR) << "Error creating table: " << m_db.lastError().text().toStdString();
            m_db.close();
            return false;
        }
    }

    if (!tables.contains("DayData")) {
        m_db.exec(QString::fromStdString(SqlQueries::createTableDayData()));
        if (m_db.lastError().type() != QSqlError::NoError) {
            LOG_S(ERROR) << "Error creating table: " << m_db.lastError().text().toStdString();
            m_db.close();
            return false;
        }
    }

    if (!tables.contains("MonthData")) {
        m_db.exec(QString::fromStdString(SqlQueries::createTableMonthData()));
        if (m_db.lastError().type() != QSqlError::NoError) {
            LOG_S(ERROR) << "Error creating table: " << m_db.lastError().text().toStdString();
            m_db.close();
            return false;
        }
    }

    m_db.close();
    return true;
}

Storage::MissingSequence SqlExporter_qt::nextMissingDayData(std::time_t now, const Serial& serial) {
    m_db.open();

    QString sql("SELECT TimeStamp FROM DayData WHERE Serial=");
    sql.append(QString::number(serial));
    sql.append(" ORDER BY TimeStamp DESC");
    auto query = m_db.exec(sql);

    int64_t to = now;
    int64_t from = 0;
    bool gapFound = false;
    while (query.next()) {
        from = query.value(0).toUInt();

        // Found a gap, break.
        if (to - from > 300) {  // SMA inverters has day data in 5 minute intervals.
            gapFound = true;
            break;
        }
        to = from;

        // If our range is out of available data range, return.
        if (to <= m_endOfDayData[serial]) {
            return {};
        }
    }

    // 2. If we have no data or there are no gaps, then request last 6 hours
    if (gapFound) {
        from = std::max(from, static_cast<int64_t>(m_endOfDayData[serial]));
    } else {
        from = std::max(static_cast<int64_t>(m_endOfDayData[serial]), to - (6 * 60 * 60) - 1);
    }
    if (to - from > 300) {
        return { from + 1, to - 1, serial };
    }

    return {};
}

Storage::MissingSequence SqlExporter_qt::nextMissingMonthData(std::time_t now, const Serial& serial) {
    m_db.open();

    QString sql("SELECT TimeStamp FROM MonthData WHERE Serial=");
    sql.append(QString::number(serial));
    sql.append(" ORDER BY TimeStamp DESC");
    auto query = m_db.exec(sql);

    int64_t to = now;
    int64_t from = 0;
    bool gapFound = false;
    while (query.next()) {
        from = query.value(0).toUInt();

        // Found a gap, break.
        if (to - from > 90000) {
            gapFound = true;
            break;
        }
        to = from;

        // If our range is out of available data range, return.
        if (to <= m_endOfMonthData[serial]) {
            return {};
        }
    }

    // 2. If we have no data or there are no gaps, then request last 62 days
    if (gapFound) {
        from = std::max(from, static_cast<int64_t>(m_endOfMonthData[serial]));
    } else {
        from = std::max(static_cast<int64_t>(m_endOfMonthData[serial]), to - (62 * 24 * 60 * 60) - 1);
    }
    if (to - from > 90000) {
        return { from + 1, to - 1, serial };
    }

    return {};
}

void SqlExporter_qt::setEndOfDayData(std::time_t timestamp, const Serial& serial) {
    if (m_endOfDayData[serial] < timestamp) {
        LOG_S(INFO) << "New end of day data for: " << serial << ", timestamp: " << timestamp;
        m_endOfDayData[serial] = timestamp;
    }
}

void SqlExporter_qt::setEndOfMonthData(std::time_t timestamp, const Serial& serial) {
    if (m_endOfMonthData[serial] < timestamp) {
        LOG_S(INFO) << "New end of month data for: " << serial << ", timestamp: " << timestamp;
        m_endOfMonthData[serial] = timestamp;
    }
}

} // namespace sql
