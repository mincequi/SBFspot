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

add_executable(bluetoothtest
    BluetoothTest.cpp
    ../Bluetooth.cpp
    ../Defines.cpp
    ../misc.cpp
    ../sunrise_sunset.cpp
    ../SBFNet.cpp
    ../Timer.cpp
)
target_link_libraries(bluetoothtest
    PkgConfig::Bluetooth
)

if (Mosquitto_FOUND AND MessagePack_FOUND)
    add_executable(mqtttest
        MqttTest.cpp
        ../Defines.cpp
        ../Export.cpp
        ../misc.cpp
        ../mqtt.cpp
        ../sunrise_sunset.cpp
        ../Timer.cpp
    )
    target_link_libraries(mqtttest
        PkgConfig::Mosquitto
        PkgConfig::MessagePack
    )
endif()