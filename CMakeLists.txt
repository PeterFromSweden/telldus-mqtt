cmake_minimum_required( VERSION 3.1 )

project( telldus-core-mqtt VERSION 0.0.2 LANGUAGES C )
set( CMAKE_C_STANDARD 11 )

# package find location
list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/" )
find_package(MOSQUITTO REQUIRED)
find_package(TELLDUS_CORE REQUIRED)
find_package(cJSON REQUIRED)

add_executable( ${PROJECT_NAME}
  "src/main.c"  
  "src/telldusclient.c" 
  "src/config.c" 
  "src/configjson.c" 
  "src/log.c"
  "src/myconsole.c"
  "src/criticalsection_stub.c"
  "src/telldusdevice.c"
  "src/telldussensor.c"
  "src/mqttclient.c" 
)

target_include_directories( ${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})
target_link_libraries( ${PROJECT_NAME} ${MOSQUITTO_LIBRARIES} ${TELLDUS_CORE_LIBRARIES}  ${CJSON_LIBRARIES})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/telldus-core-mqtt.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/telldus-sensor.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/telldus-device.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/homeassistant-sensor.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/homeassistant-device.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

install( TARGETS ${PROJECT_NAME} )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources( ${PROJECT_NAME} PRIVATE 
    "src/mythread_linux.c"
    "src/mysyslog_linux.c"
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources( ${PROJECT_NAME} PRIVATE "src/mythread_win.c")
else()
  message(FATAL_ERROR "Only Linux and Windows targets currently supported!")
endif()

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
