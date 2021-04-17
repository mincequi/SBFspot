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

#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>

#include <Config.h>
#include <LiveData.h>
#include <Logger.h>
#include <sql/SqlQueries.h>

namespace sql {

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

    m_db.setDatabaseName(QString::fromStdString(m_config.databaseName));
    m_db.setHostName(QString::fromStdString(m_config.hostName));
    m_db.setPort(m_config.port);
    m_db.setUserName(QString::fromStdString(m_config.userName));
    m_db.setPassword(QString::fromStdString(m_config.password));
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

bool SqlExporter_qt::createTables() {
    if (!m_db.open()) {
        LOG_F(ERROR, "Opening database failed");
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

    m_db.close();
    return true;
}

} // namespace sql
