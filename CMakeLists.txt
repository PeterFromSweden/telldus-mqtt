cmake_minimum_required( VERSION 3.10.2 ) # Version as per Ubuntu 18.04

project( telldus-mqtt VERSION 0.4.1 LANGUAGES C )
set( CMAKE_C_STANDARD 11 )

# package find location
list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/" )
find_package( MOSQUITTO REQUIRED )
find_package( TELLDUS_CORE REQUIRED )
find_package( cJSON REQUIRED )

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

if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  target_sources( ${PROJECT_NAME} PRIVATE 
    "src/mythread_linux.c"
    "src/mysyslog_linux.c"
    "src/mytimer_linux.c"
  )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Windows" )
  target_sources( ${PROJECT_NAME} PRIVATE 
    "src/mythread_win.c"
    "src/mysyslog_win.c"
    "src/mytimer_win.c"
  )
else()
  message( FATAL_ERROR "Only Linux and Windows targets currently supported!" )
endif()

if( MSVC )
  target_compile_definitions( ${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS )
  target_compile_options( ${PROJECT_NAME} PRIVATE /wd5105 ) # macro expansion producing 'defined' has undefined behavior
  target_compile_options( ${PROJECT_NAME} PRIVATE /wd4005 ) # macro redefinition (telldus.h)
endif()

target_include_directories( ${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})
target_link_libraries( ${PROJECT_NAME} ${MOSQUITTO_LIBRARIES} ${TELLDUS_CORE_LIBRARIES}  ${CJSON_LIBRARIES} )
if( NOT MSVC )
  target_link_libraries( ${PROJECT_NAME} rt ) # rt library for timer
endif()
target_compile_definitions( ${PROJECT_NAME} PRIVATE ASRT_LOG_ENABLE ) # Extends ASRT with logging, not for unit tests targets

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/telldus-mqtt.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
configure_file( telldus-mqtt-homeassistant.json.in telldus-mqtt-homeassistant.json @ONLY )
configure_file( src/version.h.in version.h @ONLY )
target_include_directories( ${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS} ${CMAKE_CURRENT_BINARY_DIR})

# ---- TEST ----
include( CTest ) # BUILD_TESTING=OFF skips the tests (e.g. OpenWrt)
if( BUILD_TESTING )

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
if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  target_sources( testjson PRIVATE src/mysyslog_linux.c )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Windows" )
  target_sources( testjson PRIVATE src/mysyslog_win.c )
else()
  message( FATAL_ERROR "Only Linux and Windows targets currently supported!" )
endif()
add_test( NAME testjson COMMAND testjson )
target_include_directories( testjson PRIVATE src ${CMAKE_CURRENT_BINARY_DIR} ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS} )
target_link_libraries( testjson  ${CJSON_LIBRARIES})

add_executable( teststringutils tests/teststringutils.c src/stringutils.c )
add_test( NAME teststringutils COMMAND teststringutils )
target_include_directories( teststringutils PRIVATE src ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS} )

add_executable( testmytimer tests/testmytimer.c )
if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  target_sources( testmytimer PRIVATE src/mytimer_linux.c src/mythread_linux.c )
  target_link_libraries( testmytimer PUBLIC -lrt )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Windows" )
  target_sources( testmytimer PRIVATE src/mytimer_win.c src/mythread_win.c )
else()
  message( FATAL_ERROR "Only Linux and Windows targets currently supported!" )
endif()
add_test( NAME testmytimer COMMAND testmytimer )
target_include_directories( testmytimer PRIVATE src )
endif()

# ---- INSTALL ----
install( TARGETS ${PROJECT_NAME} )
install( FILES telldus-mqtt.json DESTINATION "etc/telldus-mqtt" )
install( FILES ${CMAKE_CURRENT_BINARY_DIR}/telldus-mqtt-homeassistant.json DESTINATION "etc/telldus-mqtt" )

# ---- TELLSTICK SIMULATOR TESTS (Linux) ----
# Link the bridge against a simulated libtelldus-core instead of the real one,
# so the telldus and MQTT logic can be tested without a TellStick.
if( BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  set( TM_SIM_SOURCES
    tests/telldussim/telldussim.c
    src/telldusclient.c
    src/telldusdevice.c
    src/telldussensor.c
    src/mqttclient.c
    src/config.c
    src/configjson.c
    src/stringutils.c
    src/log.c
    src/myconsole.c
    src/mysyslog_linux.c
    src/mythread_linux.c
    src/mytimer_linux.c
    src/criticalsection_stub.c
  )
  find_package( Threads REQUIRED )

  add_executable( testtelldussim tests/testtelldussim.c ${TM_SIM_SOURCES} )
  target_include_directories( testtelldussim PRIVATE src tests ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS} )
  target_link_libraries( testtelldussim ${MOSQUITTO_LIBRARIES} ${CJSON_LIBRARIES} Threads::Threads rt )
  add_test( NAME testtelldussim COMMAND testtelldussim )

  # End-to-end test through a real broker, skipped if mosquitto is not installed
  find_program( MOSQUITTO_BROKER mosquitto PATHS /usr/sbin /usr/local/sbin )
  add_executable( testbridge tests/testbridge.c ${TM_SIM_SOURCES} )
  target_include_directories( testbridge PRIVATE src tests ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS} )
  target_link_libraries( testbridge ${MOSQUITTO_LIBRARIES} ${CJSON_LIBRARIES} Threads::Threads rt )
  add_test( NAME testbridge COMMAND testbridge "${MOSQUITTO_BROKER}" )
  set_tests_properties( testbridge PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 )
endif()
