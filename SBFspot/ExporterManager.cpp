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

#include "ExporterManager.h"

#include <Config.h>
#include <CSVexport.h>
#include <LiveData.h>
#include <SQLselect.h>
#include <mqtt.h>
#include <mqtt/MqttExporter_qt.h>
#include <sql/SqlExporter_qt.h>

ExporterManager::ExporterManager(const Config& config, Cache& cache) :
    m_config(config),
    m_cache(cache),
    m_jsonSerializer(config) {
    if (config.exporters.count(ExporterType::Csv)) {
        m_exporters.push_back(new CsvExporter(config));
    }
    if (config.exporters.count(ExporterType::Sql)) {
        //m_exporters.push_back(new db_SQL_Export(config.sql));
        m_exporters.push_back(new sql::SqlExporter_qt(config.sql));
    }
    if (config.exporters.count(ExporterType::Mqtt)) {
        if (config.mqtt_item_format == "MSGPACK") {
            m_exporters.push_back(new mqtt::MqttExporter_qt(config, m_msgPackSerializer));
        } else {
            m_exporters.push_back(new MqttExporter(config, m_jsonSerializer));
        }
    }
}

ExporterManager::~ExporterManager() {
    for (auto& exporter : m_exporters) {
        delete exporter;
    }
    m_exporters.clear();
}

bool ExporterManager::init() {
    for (const auto& exporter : m_exporters) {
        exporter->init();
    }
    return true;
}

bool ExporterManager::open() {
    for (const auto& exporter : m_exporters) {
        exporter->open();
    }
    return true;
}

void ExporterManager::close()
{
    for (const auto& exporter : m_exporters) {
        exporter->close();
    }
}

void ExporterManager::exportSpotData(std::time_t timestamp, const std::vector<InverterData>& inverters) {
    for (const auto& exporter : m_exporters) {
        if (exporter->isLive()) {
            exporter->exportSpotData(timestamp, inverters);
        }
    }

    // SQL, CSV, ... are archive exporters and will cause disk I/O.
    // These can severely exhaust disk space and shall be rate limited.
    if (!m_config.daemon ||
            (m_config.archiveInterval > 0 &&
             (timestamp % m_config.archiveInterval == 0)))
    {
        if (inverters[0].DevClass == SolarInverter && m_config.nospot == 0)
        {
            for (const auto& exporter : m_exporters) {
                if (!exporter->isLive()) {
                    exporter->exportSpotData(timestamp, inverters);
                }
            }
        }

        if (hasBatteryDevice && (m_config.nospot == 0)) {
            for (const auto& exporter : m_exporters) {
                exporter->exportBatteryData(timestamp, inverters);
            }
        }

        // TODO: reimplement exporting historic data via MQTT
        // if (m_config.mqtt == 1)
        // {
        //     // Create timestamp for start of day
        //     std::tm* lt = std::localtime(&timestamp);
        //     lt->tm_hour = 0;
        //     lt->tm_min = 0;
        //     lt->tm_sec = 0;
        //     auto tsStart = mktime(lt);
        //     // Get data from DB
        //     auto data = m_db.getInverterData(tsStart, timestamp);
        //     m_mqtt.exportDayData(tsStart, data);
        // }
    }
}

void ExporterManager::exportConfig(const InverterData& inverterData) {
    for (const auto& exporter : m_exporters) {
        exporter->exportConfig(inverterData);
    }
}

void ExporterManager::exportLiveData(const LiveData& liveData) {
    for (auto& exporter : m_exporters) {
        // Live exporters always export.
        // Non-live exporter only export when timestamp matches archive interval.
        if (exporter->isLive()) {
            exporter->exportLiveData(liveData);
        } else if ((m_config.archiveInterval > 0 &&
                    (liveData.timestamp % m_config.archiveInterval == 0))) {
            exporter->exportLiveData(liveData);
        }
    }
}

void ExporterManager::exportDayStats(const DayStats& dayStats) {
    for (auto& exporter : m_exporters) {
        exporter->exportDayStats(dayStats);
    }
}

void ExporterManager::exportDayData(const std::vector<InverterData>& inverters) {
    for (auto& exporter : m_exporters) {
        exporter->exportDayData(inverters);
    }
}

void ExporterManager::exportMonthData(const std::vector<InverterData>& inverters) {
    for (auto& exporter : m_exporters) {
        exporter->exportMonthData(inverters);
    }
}

void ExporterManager::exportEventData(const std::vector<InverterData>& inverters, const std::string& dt_range_csv) {
    for (auto& exporter : m_exporters) {
        exporter->exportEventData(inverters, dt_range_csv);
    }
}

/*
void ExporterManager::exportSpotDataMqtt(std::time_t timestamp, const std::vector<InverterData>& inverters) {
    // Compute statistics
    m_dayStats.resize(inverters.size());
    for (uint32_t i = 0; i < inverters.size(); ++i)
    {
        bool isDirty = false;
        m_dayStats[i].serial = inverters[i].Serial;
        if (m_dayStats[i].powerMax < inverters[i].Pac1)
        {
            m_dayStats[i].powerMax = inverters[i].Pac1;
            isDirty = true;
        }
        m_dayStats[i].stringPowerMax.resize(2);
        if (m_dayStats[i].stringPowerMax[0] < inverters[i].Pdc1)
        {
            m_dayStats[i].stringPowerMax[0] = inverters[i].Pdc1;
            isDirty = true;
        }
        if (m_dayStats[i].stringPowerMax[1] < inverters[i].Pdc2)
        {
            m_dayStats[i].stringPowerMax[1] = inverters[i].Pdc2;
            isDirty = true;
        }
        if (isDirty)
        {
            m_dayStats[i].timestamp = timestamp;
            exportDayStats(m_dayStats[i]);
        }
    }

    auto rc = m_mqtt.exportLiveData(timestamp, inverters);
    if (rc != 0)
    {
        std::cout << "Error " << rc << " while publishing to MQTT Broker" << std::endl;
    }
}
*/