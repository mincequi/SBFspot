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

#include "MqttExport_qt.h"

#include <QHostAddress>
#include <qmqtt_message.h>

#include "Config.h"
#include "LiveData.h"
#include "Serializer.h"

namespace mqtt {

MqttExport_qt::MqttExport_qt(const Config& config, const Serializer& serializer)
    : m_config(config),
      m_serializer(serializer),
      m_client(QString::fromStdString(config.mqtt_host), config.mqtt_port, false, false)
{
    QObject::connect(&m_client, &QMQTT::Client::error, this, &MqttExport_qt::onError);
    m_client.connectToHost();
}

MqttExport_qt::~MqttExport_qt()
{
}

std::string MqttExport_qt::name() const
{
    return "MqttExport_qt";
}

int MqttExport_qt::exportLiveData(std::time_t /*timestamp*/,
                                  const std::vector<InverterData>& /*inverterData*/)
{
    return 0;
}

int MqttExport_qt::exportLiveData(const LiveData& liveData)
{
    if (!m_client.isConnectedToHost()) {
        m_client.connectToHost();
    }

    static quint16 id = 0;
    std::string topic = m_config.mqtt_topic;
    boost::replace_first(topic, "{plantname}", m_config.plantname);
    boost::replace_first(topic, "{serial}", std::to_string(liveData.serial));
    topic += "/live";

    auto data = m_serializer.serialize(liveData);
    QMQTT::Message message(++id,
                           QString::fromStdString(topic),
                           QByteArray::fromRawData(data.data(), data.size()),
                           0,
                           true);

    m_client.publish(message);

    return 0;
}

void MqttExport_qt::onError(const QMQTT::ClientError error)
{
    qDebug() << "MQTT error:" << error;
}

}