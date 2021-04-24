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

#include "mqtt.h"

#include "Config.h"
#include "Serializer.h"
#include "TagDefs.h"
#include "misc.h"

MqttExporter::MqttExporter(const Config& config, const Serializer& serializer) :
    m_config(config),
    m_serializer(serializer)
{
}

MqttExporter::~MqttExporter()
{
}

std::string MqttExporter::name() const
{
    return "MqttExport";
}

void MqttExporter::exportSpotData(std::time_t timestamp, const std::vector<InverterData> &inverterData)
{
    for (const auto& inv : inverterData)
	{
#if defined(WIN32)
        std::string mqtt_command_line = "\"\"" + m_config.mqtt_publish_exe + "\" " + m_config.mqtt_publish_args + "\"";
#else
        std::string mqtt_command_line = m_config.mqtt_publish_exe + " " + m_config.mqtt_publish_args;
		// On Linux, message must be inside single quotes
		boost::replace_all(mqtt_command_line, "\"", "'");
#endif

		// Fill host/port/topic
        boost::replace_first(mqtt_command_line, "{host}", m_config.mqtt_host);
        boost::replace_first(mqtt_command_line, "{port}", std::to_string(m_config.mqtt_port));
        boost::replace_first(mqtt_command_line, "{topic}", m_config.mqtt_topic);

        auto buffer = m_serializer.serialize(inv);
        std::string str(buffer.begin(), buffer.end());

        if (VERBOSE_NORMAL) std::cout << "MQTT: Publishing (" << m_config.mqtt_topic << ") " << str << std::endl;

		std::stringstream serial;
		serial.str("");
        serial << inv.Serial;
        boost::replace_first(mqtt_command_line, "{plantname}", m_config.plantname);
		boost::replace_first(mqtt_command_line, "{serial}", serial.str());
        boost::replace_first(mqtt_command_line, "{message}", str);

		int system_rc = ::system(mqtt_command_line.c_str());
		if (system_rc != 0) // Error
		{
            std::cout << "MQTT: Failed to execute '" << m_config.mqtt_publish_exe << "' mosquitto clients installed?" << std::endl;
		}
    } // for (const auto& inv : inverterData)
}

