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

#include "MqttMsgPackExport.h"

#include "Config.h"
#include "SBFspot.h"

#include <chrono>
#include <thread>

using namespace std::chrono_literals;

MqttMsgPackExport::MqttMsgPackExport(const Config& config)
    : m_config(config)
{
    mosqpp::lib_init();
}

MqttMsgPackExport::~MqttMsgPackExport()
{
    disconnectFromHost();
    mosqpp::lib_cleanup();
}

std::string MqttMsgPackExport::name() const
{
    return "MqttMsgPackExport";
}

int MqttMsgPackExport::exportConfig(const std::vector<InverterData>& inverterData)
{
    connectToHost();

    // Collect PV array config per serial
    std::multimap<uint32_t,StringConfig> arrayConfig;
    for (const auto& ac : m_config.pvArrays)
        arrayConfig.insert({ac.inverterSerial, ac});

    for (const auto& inv : inverterData)
    {
        std::string topic = m_config.mqtt_topic;
        boost::replace_first(topic, "{plantname}", m_config.plantname);
        boost::replace_first(topic, "{serial}", std::to_string(inv.Serial));
        topic += "/config";

        // Pack manually (because a float in map gets stored as double and timestamp is not supported yet).
        msgpack::sbuffer sbuf;
        msgpack::packer<msgpack::sbuffer> packer(sbuf);
        // Map with number of elements
        packer.pack_map(6);
        // 1. Protocol version
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Version));
        packer.pack_uint8(0);
        // 2. Device name
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Name));
        std::string str = inv.DeviceName;
        packer.pack_str(str.size());
        packer.pack_str_body(inv.DeviceName, str.size());
        // 3. Latitude
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Latitude));
        packer.pack_float(m_config.latitude);
        // 4. Longitude
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Longitude));
        packer.pack_float(m_config.longitude);
        // 5. Power Max
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::PowerMax));
        packer.pack_float(static_cast<float>(inv.Pmax1));
        // 6. Array config
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Strings));
        packer.pack_array(arrayConfig.count(inv.Serial));   // Store an array to provide data for each PV array.

        auto itb = arrayConfig.lower_bound(inv.Serial);
        auto ite = arrayConfig.upper_bound(inv.Serial);
        for (auto it = itb; it != ite; ++it)
        {
            packer.pack_map(4);
            packer.pack_uint8(static_cast<uint8_t>(InverterProperty::StringName));
            packer.pack((*it).second.name);
            packer.pack_uint8(static_cast<uint8_t>(InverterProperty::StringAzimuth));
            packer.pack_float(static_cast<float>((*it).second.azimuth));
            packer.pack_uint8(static_cast<uint8_t>(InverterProperty::StringElevation));
            packer.pack_float(static_cast<float>((*it).second.elevation));
            packer.pack_uint8(static_cast<uint8_t>(InverterProperty::StringPowerMax));
            packer.pack_float(static_cast<float>((*it).second.powerPeak));
        }

        publish(topic, sbuf, 1);
    }

    return 0;
}

int MqttMsgPackExport::exportDayStats(std::time_t timestamp,
                                      const std::vector<DayStats>& dayStats)
{
    connectToHost();

    for (const auto& stats : dayStats)
    {
        std::string topic = m_config.mqtt_topic;
        boost::replace_first(topic, "{plantname}", m_config.plantname);
        boost::replace_first(topic, "{serial}", std::to_string(stats.serial));
        topic += "/today/stats";

        // Pack manually (because a float in map gets stored as double and timestamp is not supported yet).
        msgpack::sbuffer sbuf;
        msgpack::packer<msgpack::sbuffer> packer(sbuf);
        // Map with number of elements
        packer.pack_map(4);
        // 1. Protocol version
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Version));
        packer.pack_uint8(0);
        // 2. Timestamp
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Timestamp));
        auto t = htonl(timestamp);
        packer.pack_ext(4, -1); // Timestamp type
        packer.pack_ext_body((const char*)(&t), 4);
        // 3. Power max today
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::PowerMaxToday));
        packer.pack_float(stats.powerMax);
        // 4. Power DC
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Strings));
        packer.pack_array(stats.stringPowerMax.size());   // Store an array to provide data for each Mpp.

        for (uint32_t i = 0; i < stats.stringPowerMax.size(); ++i)
        {
            // 4.X
            packer.pack_map(1);
            packer.pack_uint8(static_cast<uint8_t>(InverterProperty::StringPowerMaxToday));
            packer.pack_float(static_cast<float>(stats.stringPowerMax[i]));
        }

        publish(topic, sbuf, 1);
    }

    return 0;
}

int MqttMsgPackExport::exportLiveData(std::time_t timestamp,
                                      const std::vector<InverterData>& inverterData)
{
    connectToHost();

    for (const auto& inv : inverterData)
    {
        std::string topic = m_config.mqtt_topic;
        boost::replace_first(topic, "{plantname}", m_config.plantname);
        boost::replace_first(topic, "{serial}", std::to_string(inv.Serial));
        topic += "/live";

        // Pack manually (because a float in map gets stored as double and timestamp is not supported yet).
        msgpack::sbuffer sbuf;
        msgpack::packer<msgpack::sbuffer> packer(sbuf);
        // Map with number of elements
        packer.pack_map(6);
        // 1. Protocol version
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Version));
        packer.pack_uint8(0);
        // 2. Timestamp
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Timestamp));
        auto t = htonl(timestamp);
        packer.pack_ext(4, -1); // Timestamp type
        packer.pack_ext_body((const char*)(&t), 4);
        // 3. Yield Total
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::YieldTotal));
        packer.pack_float(static_cast<float>(inv.ETotal));
        // 4. Yield Today
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::YieldToday));
        packer.pack_float(static_cast<float>(inv.EToday));
        // 5. Power AC
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Power));
        packer.pack_float(static_cast<float>(inv.TotalPac));
        // 6. Power DC
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Strings));
        packer.pack_array(2);   // Store an array to provide data for each Mpp.
        // 6.1 MPP1
        packer.pack_map(1);
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Power));
        packer.pack_float(static_cast<float>(inv.Pdc1));
        // 6.2 MPP2
        packer.pack_map(1);
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Power));
        packer.pack_float(static_cast<float>(inv.Pdc2));

        publish(topic, sbuf, 0);
    }

    return 0;
}

int MqttMsgPackExport::exportDayData(std::time_t timestamp, const DataPerInverter& inverterData)
{
    connectToHost();

    for (const auto& inv : inverterData)
    {
        std::string topic = m_config.mqtt_topic;
        boost::replace_first(topic, "{plantname}", m_config.plantname);
        boost::replace_first(topic, "{serial}", std::to_string(inv.first));
        topic += "/day/data";

        // Pack manually (because a float in map gets stored as double and timestamp is not supported yet).
        msgpack::sbuffer sbuf;
        msgpack::packer<msgpack::sbuffer> packer(sbuf);
        // Map with number of elements
        packer.pack_map(3);
        // 1. Protocol version
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Version));
        packer.pack_uint8(0);
        // 2. Timestamp
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Timestamp));
        packer.pack_array(inv.second.size());
        for (const auto& p : inv.second) {
            auto t = htonl(p.InverterDatetime);
            packer.pack_ext(4, -1); // Timestamp type
            packer.pack_ext_body((const char*)(&t), 4);
        }
        // 3. Power AC
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Power));
        packer.pack_array(inv.second.size());
        for (const auto& p : inv.second) {
            packer.pack_unsigned_int(p.TotalPac);
        }
        // 4. Power DC
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Strings));
        packer.pack_array(2);   // Store an array to provide data for each Mpp.
        // 4.1 MPP1
        packer.pack_map(1);
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Power));
        packer.pack_array(inv.second.size());
        for (const auto& p : inv.second) {
            packer.pack_unsigned_int(p.Pdc1);
        }
        // 4.2 MPP2
        packer.pack_map(1);
        packer.pack_uint8(static_cast<uint8_t>(InverterProperty::Power));
        packer.pack_array(inv.second.size());
        for (const auto& p : inv.second) {
            packer.pack_unsigned_int(p.Pdc2);
        }

        publish(topic, sbuf, 0);
    }

    return 0;
}

void MqttMsgPackExport::connectToHost()
{
    if (m_isConnected)
    {
        return;
    }

    if (VERBOSE_HIGH)
        std::cout << "MQTT: Connecting broker: " << m_config.mqtt_host << std::endl;
    if (connect(m_config.mqtt_host.c_str()) != 0)
    {
        std::cout << "MQTT: Failed to connect broker: " << m_config.mqtt_host << std::endl;
        return;
    }

    // Do NOT start loop before being connected.
    loop_start();
}

void MqttMsgPackExport::disconnectFromHost()
{
    // Let's sleep a little bit. mosquitto expects to run in an event loop.
    std::this_thread::sleep_for(100ms);
    disconnect();
    loop_stop();
}

void MqttMsgPackExport::publish(const std::string& topic, const msgpack::sbuffer& buffer, uint8_t qos)
{
    if (VERBOSE_HIGH) std::cout << "MQTT: Publishing topic: " << topic
                                << ", data size: " << buffer.size() << std::endl;

    int rc = 0;
    rc = mosqpp::mosquittopp::publish(nullptr, topic.c_str(), buffer.size(), buffer.data(), qos, true);
    if (rc != 0)
    {
        std::cout << "MQTT: Failed to publish topic: " << topic << ", error: " << mosqpp::strerror(rc) << std::endl;
    }
}

void MqttMsgPackExport::on_connect(int rc)
{
    if (rc == 0) m_isConnected = true;
}

void MqttMsgPackExport::on_disconnect(int)
{
    m_isConnected = false;
}
