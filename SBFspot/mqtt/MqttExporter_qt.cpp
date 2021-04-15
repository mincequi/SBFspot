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

#include "MqttExporter_qt.h"

#include <QHostAddress>
#include <qmqtt_message.h>

#include "Config.h"
#include "LiveData.h"
#include "Logging.h"
#include "Serializer.h"

namespace mqtt {

static quint16 msgId = 0;

MqttExporter_qt::MqttExporter_qt(const Config& config, const Serializer& serializer)
    : m_config(config),
      m_serializer(serializer),
      m_client(QString::fromStdString(config.mqtt_host), config.mqtt_port, false, false)
{
    QObject::connect(&m_client, &QMQTT::Client::error, this, &MqttExporter_qt::onError);
    m_client.connectToHost();
}

MqttExporter_qt::~MqttExporter_qt()
{
}

std::string MqttExporter_qt::name() const
{
    return "MqttExporter_qt";
}

bool MqttExporter_qt::isLive() const
{
    return true;
}

void MqttExporter_qt::exportLiveData(const LiveData& liveData)
{
    if (!m_client.isConnectedToHost()) {
        m_client.connectToHost();
    }

    std::string topic = m_config.mqtt_topic;
    boost::replace_first(topic, "{plantname}", m_config.plantname);
    boost::replace_first(topic, "{serial}", std::to_string(liveData.serial));
    topic += "/live";

    auto data = m_serializer.serialize(liveData);
    QMQTT::Message message(++msgId,
                           QString::fromStdString(topic),
                           QByteArray::fromRawData(reinterpret_cast<const char*>(data.data()), data.size()),
                           0,
                           true);

    LOG_F(1, "Publishing topic: %s, payload size: %zu bytes", topic.c_str(), data.size());
    m_client.publish(message);
}

void MqttExporter_qt::exportSpotData(std::time_t /*timestamp*/, const std::vector<InverterData>& inverters)
{
    if (!m_client.isConnectedToHost()) {
        m_client.connectToHost();
    }

    for (const auto& inverterData : inverters) {
        auto buffer = m_serializer.serialize(inverterData);
        if (buffer.empty()) continue;

        std::string topic = m_config.mqtt_topic;
        boost::replace_first(topic, "{plantname}", m_config.plantname);
        boost::replace_first(topic, "{serial}", std::to_string(inverterData.Serial));

        QMQTT::Message message(++msgId,
                               QString::fromStdString(topic),
                               QByteArray::fromRawData(reinterpret_cast<const char*>(buffer.data()), buffer.size()),
                               0,
                               true);

        LOG_F(1, "Publishing topic: %s, payload size: %zu bytes", topic.c_str(), buffer.size());
        m_client.publish(message);
    }
}

void MqttExporter_qt::onError(const QMQTT::ClientError error)
{
    LOG_S(WARNING) << "Client error:" << error;
}

}
