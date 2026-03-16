idf_component_register(
    SRCS
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Storage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/AuthenticationController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CommonSetup.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/CurrentDateTimeController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Result.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/StringUtil.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Messages/MessageLoginRequest.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Messages/MessageRegisterDevice.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Messages/MessageRegisterSensorType.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Messages/MessageRegisterTaskType.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Messages/MessageSensorData.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Messages/MessageState.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/Parameters/Parameter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Parameters/ParameterInteger.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Parameters/ParameterTime.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Parameters/StatefulParameterSwitch.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Parameters/StatefulParameterSlider.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/Sensors/SensorsController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Sensors/SensorType.cpp

        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tasks/TaskBase.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tasks/TaskController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tasks/TaskProgressUpdate.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/Tasks/TaskType.cpp
        
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/NetworkManager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/connection_provider/NetworkConnectionProvider.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/connection_provider/Sim7600NetworkConnectionController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/connection_provider/WifiNetworkConnectionController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/http/HttpProvider.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/http/Sim7600HttpController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/http/WifiHttpController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/mqtt/MqttProvider.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/mqtt/Sim7600MqttController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/mqtt/WifiMqttController.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/network/sim7600/Sim7600.cpp

    INCLUDE_DIRS
        src
        src/Messages
        src/Parameters
        src/Sensors
        src/Tasks
        src/network
        src/network/connection_provider
        src/network/http
        src/network/mqtt
        src/network/sim7600

    REQUIRES
        nvs_flash
        mqtt
        json
        joltwallet__littlefs
        esp_http_client
        driver
        esp_wifi
        espp
        #vfs_littlefs
        #esp_littlefs
        #esp_littlefs
        #joltwallet__littlefs
)
