#add_executable(pluginmanagertest
#    PluginManagerTest.cpp
#    ../PluginManager.cpp
#    ../ble/BleExport.cpp
#)

#target_include_directories(pluginmanagertest
#PRIVATE
#    ../
#)

#target_link_libraries(pluginmanagertest
#    bluez-dbus-cpp
#)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

include_directories(
    ../
)

add_executable(cachetest
    CacheTest.cpp
    ../EventData.cpp
    ../Cache.cpp
    ../Types.cpp
)

#if (Bluetooth_FOUND)
#    add_executable(bluetoothtest
#        BluetoothTest.cpp
#        ../Bluetooth.cpp
#        ../Defines.cpp
#        ../Ethernet.cpp
#        ../Importer.cpp
#        ../misc.cpp
#        ../sunrise_sunset.cpp
#        ../SBFNet.cpp
#        ../SBFspot.cpp
#        ../Timer.cpp
#        ../sma/SmaInverterRequests.cpp
#    )
#    target_link_libraries(bluetoothtest
#        PkgConfig::Bluetooth
#    )
#endif()

if (Mosquitto_FOUND AND MessagePack_FOUND)
    add_executable(mqttexportertest
        MqttExporterTest.cpp
        ../Defines.cpp
        ../Exporter.cpp
        ../misc.cpp
        ../MqttMsgPackExporter.cpp
        ../sunrise_sunset.cpp
        ../TagDefs.cpp
        ../Timer.cpp
        ../Types.cpp
    )
    target_link_libraries(mqttexportertest
        Boost::date_time
        PkgConfig::Mosquitto
        PkgConfig::MessagePack
    )

    add_executable(mqttsubscribertest
        MqttSubscriberTest.cpp
        ../Defines.cpp
        ../Exporter.cpp
        ../misc.cpp
        ../MqttMsgPackExporter.cpp
        ../sunrise_sunset.cpp
        ../TagDefs.cpp
        ../Timer.cpp
        ../Types.cpp
    )
    target_link_libraries(mqttsubscribertest
        Boost::date_time
        PkgConfig::Mosquitto
        PkgConfig::MessagePack
    )
endif()
