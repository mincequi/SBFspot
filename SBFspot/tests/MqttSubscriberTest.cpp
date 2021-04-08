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

#include <algorithm>
#include <iostream>
#include <string>
#include <thread>


#include <msgpack.hpp>
#include <mqtt_protocol.h>
#include <mosquittopp.h>

class MqttSubscriber : public mosqpp::mosquittopp
{
public:
    MqttSubscriber(const std::string& topic) :
        m_topic(topic)
    {
        connect("broker.hivemq.com");
        subscribe(nullptr, topic.c_str());
    }

private:
    virtual void on_message(const struct mosquitto_message* message) override {
        auto oh = msgpack::unpack(reinterpret_cast<const char*>(message->payload), message->payloadlen);

        // deserialized object is valid during the msgpack::object_handle instance is alive.
        msgpack::object deserialized = oh.get();

        // msgpack::object supports ostream.
        std::cout << deserialized << std::endl;
    }

    const std::string m_topic;

};

int main()
{
    mosqpp::lib_init();

    MqttSubscriber client("sbfspot_1900042748/live");
    client.loop_forever();

    mosqpp::lib_cleanup();

    return 0;
}
