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

#include "JsonSerializer.h"

#include "Config.h"
#include "Logging.h"
#include "TagDefs.h"
#include "Types.h"
#include "misc.h"

namespace json {

JsonSerializer::JsonSerializer(const Config& config) :
    m_config(config) {
}

JsonSerializer::~JsonSerializer() {
}

ByteBuffer JsonSerializer::serialize(const InverterData& inverterData) const {
    // Split message body
    std::vector<std::string> items;
    boost::split(items, m_config.mqtt_publish_data, boost::is_any_of(","));

    std::stringstream mqtt_message;
    std::string key;
    char value[80];
    int prec = m_config.precision;
    char dp = '.';

    mqtt_message.str("");

    for (std::vector<std::string>::iterator it = items.begin(); it != items.end(); ++it)
    {
        time_t timestamp = time(NULL);
        key = *it;
        memset(value, 0, sizeof(value));
        std::transform((key).begin(), (key).end(), (key).begin(), ::tolower);
        if (key == "timestamp")				snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(m_config.DateTimeFormat, timestamp));
        else if (key == "sunrise")			snprintf(value, sizeof(value) - 1, "\"%s %02d:%02d:00\"", strftime_t(m_config.DateFormat, timestamp), (int)m_config.sunrise, (int)((m_config.sunrise - (int)m_config.sunrise) * 60));
        else if (key == "sunset")			snprintf(value, sizeof(value) - 1, "\"%s %02d:%02d:00\"", strftime_t(m_config.DateFormat, timestamp), (int)m_config.sunset, (int)((m_config.sunset - (int)m_config.sunset) * 60));
        else if (key == "invserial")		snprintf(value, sizeof(value) - 1, "%lu", inverterData.Serial);
        else if (key == "invname")			snprintf(value, sizeof(value) - 1, "\"%s\"", inverterData.DeviceName.c_str());
        else if (key == "invclass")			snprintf(value, sizeof(value) - 1, "\"%s\"", inverterData.DeviceClass.c_str());
        else if (key == "invtype")			snprintf(value, sizeof(value) - 1, "\"%s\"", inverterData.DeviceType.c_str());
        else if (key == "invswver")			snprintf(value, sizeof(value) - 1, "\"%s\"", inverterData.SWVersion.c_str());
        else if (key == "invtime")			snprintf(value, sizeof(value) - 1, "\"%s\"", strftime_t(m_config.DateTimeFormat, inverterData.InverterDatetime));
        else if (key == "invstatus")		snprintf(value, sizeof(value) - 1, "\"%s\"", tagdefs.getDesc(inverterData.DeviceStatus, "?").c_str());
        else if (key == "invtemperature")	FormatFloat(value, (float)inverterData.Temperature / 100, 0, prec, dp);
        else if (key == "invgridrelay")		snprintf(value, sizeof(value) - 1, "\"%s\"", tagdefs.getDesc(inverterData.GridRelayStatus, "?").c_str());
        else if (key == "pdc1")				FormatFloat(value, (float)inverterData.Pdc1, 0, prec, dp);
        else if (key == "pdc2")				FormatFloat(value, (float)inverterData.Pdc2, 0, prec, dp);
        else if (key == "idc1")				FormatFloat(value, (float)inverterData.Idc1 / 1000, 0, prec, dp);
        else if (key == "idc2")				FormatFloat(value, (float)inverterData.Idc2 / 1000, 0, prec, dp);
        else if (key == "udc1")				FormatFloat(value, (float)inverterData.Udc1 / 100, 0, prec, dp);
        else if (key == "udc2")				FormatFloat(value, (float)inverterData.Udc2 / 100, 0, prec, dp);
        else if (key == "etotal")			FormatDouble(value, (double)inverterData.ETotal / 1000, 0, prec, dp);
        else if (key == "etoday")			FormatDouble(value, (double)inverterData.EToday / 1000, 0, prec, dp);
        else if (key == "pactot")			FormatFloat(value, (float)inverterData.TotalPac, 0, prec, dp);
        else if (key == "pac1")				FormatFloat(value, (float)inverterData.Pac1, 0, prec, dp);
        else if (key == "pac2")				FormatFloat(value, (float)inverterData.Pac2, 0, prec, dp);
        else if (key == "pac3")				FormatFloat(value, (float)inverterData.Pac3, 0, prec, dp);
        else if (key == "uac1")				FormatFloat(value, (float)inverterData.Uac1 / 100, 0, prec, dp);
        else if (key == "uac2")				FormatFloat(value, (float)inverterData.Uac2 / 100, 0, prec, dp);
        else if (key == "uac3")				FormatFloat(value, (float)inverterData.Uac3 / 100, 0, prec, dp);
        else if (key == "iac1")				FormatFloat(value, (float)inverterData.Iac1 / 1000, 0, prec, dp);
        else if (key == "iac2")				FormatFloat(value, (float)inverterData.Iac2 / 1000, 0, prec, dp);
        else if (key == "iac3")				FormatFloat(value, (float)inverterData.Iac3 / 1000, 0, prec, dp);
        else if (key == "gridfreq")			FormatFloat(value, (float)inverterData.GridFreq / 100, 0, prec, dp);
        else if (key == "opertm")			FormatDouble(value, (double)inverterData.OperationTime / 3600, 0, prec, dp);
        else if (key == "feedtm")			FormatDouble(value, (double)inverterData.FeedInTime / 3600, 0, prec, dp);
        else if (key == "battmpval")		FormatFloat(value, ((float)inverterData.BatTmpVal) / 10, 0, prec, dp);
        else if (key == "batvol")			FormatFloat(value, ((float)inverterData.BatVol) / 100, 0, prec, dp);
        else if (key == "batamp")			FormatFloat(value, ((float)inverterData.BatAmp) / 1000, 0, prec, dp);
        else if (key == "batchastt")		FormatFloat(value, ((float)inverterData.BatChaStt), 0, prec, dp);

        // None of the above, so it's an unhandled item or a typo...
        else LOG_S(INFO) << "Don't know what to do with '" << *it << "'" << std::endl;

        std::string key_value = m_config.mqtt_item_format;
        boost::replace_all(key_value, "{key}", (*it));
        boost::replace_first(key_value, "{value}", value);

        boost::replace_all(key_value, "\"\"", "\"");
#if defined(WIN32)
        boost::replace_all(key_value, "\"", "\"\"");
#endif

        // Append delimiter, except for first item. Then add opening braces
        if (!mqtt_message.str().empty()) {
            mqtt_message << m_config.mqtt_item_delimiter;
        } else {
            mqtt_message << "{";
        }

        mqtt_message << key_value;
    }

    if (!mqtt_message.str().empty()) {
        mqtt_message << "}";
    }

    const std::string str = mqtt_message.str();
    return {str.begin(), str.end()};
}

} // namespace json
