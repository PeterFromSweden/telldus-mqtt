cmake_minimum_required( VERSION 3.1 )

project( telldus-mqtt VERSION 0.2 LANGUAGES C )
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
  "src/stringutils.c" 
  "src/telldusdevice.c"
  "src/telldussensor.c"
  "src/mqttclient.c" 
)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources( ${PROJECT_NAME} PRIVATE 
    "src/mythread_linux.c"
    "src/mysyslog_linux.c"
    "src/mytimer_linux.c"
  )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources( ${PROJECT_NAME} PRIVATE 
    "src/mythread_win.c"
    "src/mysyslog_win.c"
    "src/mytimer_win.c"
  )
else()
  message(FATAL_ERROR "Only Linux and Windows targets currently supported!")
endif()

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

target_include_directories( ${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})
target_link_libraries( ${PROJECT_NAME} ${MOSQUITTO_LIBRARIES} ${TELLDUS_CORE_LIBRARIES}  ${CJSON_LIBRARIES} -lrt)

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/telldus-mqtt.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
configure_file( telldus-mqtt-homeassistant.json.in telldus-mqtt-homeassistant.json @ONLY )
configure_file( src/version.h.in version.h @ONLY )
target_include_directories( ${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

# ---- TEST ----
include( CTest )

add_executable( testconfig tests/testconfig.c src/config.c src/configjson.c src/stringutils.c src/log.c src/myconsole.c )
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources( testconfig PRIVATE src/mysyslog_linux.c )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources( testconfig PRIVATE src/mysyslog_win.c )
else()
  message(FATAL_ERROR "Only Linux and Windows targets currently supported!")
endif()
add_test( NAME testconfig COMMAND testconfig )
target_include_directories( testconfig PRIVATE src ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})
target_link_libraries( testconfig  ${CJSON_LIBRARIES})

add_executable( testjson tests/testjson.c src/configjson.c src/stringutils.c src/log.c src/myconsole.c )
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources( testjson PRIVATE src/mysyslog_linux.c )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources( testjson PRIVATE src/mysyslog_win.c )
else()
  message(FATAL_ERROR "Only Linux and Windows targets currently supported!")
endif()
add_test( NAME testjson COMMAND testjson )
target_include_directories( testjson PRIVATE src ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})
target_link_libraries( testjson  ${CJSON_LIBRARIES})

add_executable( teststringutils tests/teststringutils.c src/stringutils.c )
add_test( NAME teststringutils COMMAND teststringutils )
target_include_directories( teststringutils PRIVATE src ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})

add_executable( testmytimer tests/testmytimer.c )
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_sources( testmytimer PRIVATE src/mytimer_linux.c src/mythread_linux.c )
  target_link_libraries( testmytimer PUBLIC -lrt )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_sources( testmytimer PRIVATE src/mytimer_win.c src/mythread_win.c )
else()
  message(FATAL_ERROR "Only Linux and Windows targets currently supported!")
endif()
add_test( NAME testmytimer COMMAND testmytimer )
target_include_directories( testmytimer PRIVATE src )

# ---- INSTALL ----
install( TARGETS ${PROJECT_NAME} )
install( FILES telldus-mqtt.json DESTINATION "etc/telldus-mqtt" )
install( FILES ${CMAKE_CURRENT_BINARY_DIR}/telldus-mqtt-homeassistant.json DESTINATION "etc/telldus-mqtt" )
