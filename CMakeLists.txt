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
  "src/stringutils.c" 
  "src/telldusdevice.c"
  "src/telldussensor.c"
  "src/mqttclient.c" 
  "src/status.c"
)

if( CMAKE_SYSTEM_NAME STREQUAL "Linux" )
  target_sources( ${PROJECT_NAME} PRIVATE 
    "src/mythread_linux.c"
    "src/mysyslog_linux.c"
    "src/mytimer_linux.c"
    "src/criticalsection_linux.c"
  )
  find_package( Threads REQUIRED )
  target_link_libraries( ${PROJECT_NAME} Threads::Threads )
elseif( CMAKE_SYSTEM_NAME STREQUAL "Windows" )
  target_sources( ${PROJECT_NAME} PRIVATE 
    "src/mythread_win.c"
    "src/mysyslog_win.c"
    "src/mytimer_win.c"
    "src/criticalsection_win.c"
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

# --config <path> on the command line
add_test( NAME cli-config-missing-arg COMMAND ${PROJECT_NAME} --nodaemon --config )
set_tests_properties( cli-config-missing-arg PROPERTIES WILL_FAIL TRUE )
add_test( NAME cli-config-no-file COMMAND ${PROJECT_NAME} --nodaemon --config no-such-dir/telldus-mqtt.json )
set_tests_properties( cli-config-no-file PROPERTIES WILL_FAIL TRUE )
add_test( NAME cli-config-no-file-msg COMMAND ${PROJECT_NAME} --nodaemon --config no-such-dir/telldus-mqtt.json )
set_tests_properties( cli-config-no-file-msg PROPERTIES PASS_REGULAR_EXPRESSION "no-such-dir/telldus-mqtt.json No such file" )
add_test( NAME cli-status-missing-arg COMMAND ${PROJECT_NAME} --nodaemon --status )
set_tests_properties( cli-status-missing-arg PROPERTIES WILL_FAIL TRUE )
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
    src/criticalsection_linux.c
    src/status.c
  )
  find_package( Threads REQUIRED )

  add_executable( testtelldussim tests/testtelldussim.c ${TM_SIM_SOURCES} )
  target_include_directories( testtelldussim PRIVATE src tests ${CMAKE_CURRENT_BINARY_DIR} ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS} )
  target_link_libraries( testtelldussim ${MOSQUITTO_LIBRARIES} ${CJSON_LIBRARIES} Threads::Threads rt )
  add_test( NAME testtelldussim COMMAND testtelldussim )

  # End-to-end test through a real broker, skipped if mosquitto is not installed
  find_program( MOSQUITTO_BROKER mosquitto PATHS /usr/sbin /usr/local/sbin )
  add_executable( testbridge tests/testbridge.c ${TM_SIM_SOURCES} )
  target_include_directories( testbridge PRIVATE src tests ${CMAKE_CURRENT_BINARY_DIR} ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS} )
  target_link_libraries( testbridge ${MOSQUITTO_LIBRARIES} ${CJSON_LIBRARIES} Threads::Threads rt )
  add_test( NAME testbridge COMMAND testbridge "${MOSQUITTO_BROKER}" translation )
  add_test( NAME testbridge-devices COMMAND testbridge "${MOSQUITTO_BROKER}" devices )
  add_test( NAME testbridge-filter COMMAND testbridge "${MOSQUITTO_BROKER}" filter )
  set_tests_properties( testbridge testbridge-devices testbridge-filter PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 )

  add_executable( teststatus tests/teststatus.c ${TM_SIM_SOURCES} )
  target_include_directories( teststatus PRIVATE src tests ${CMAKE_CURRENT_BINARY_DIR} ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS} )
  target_link_libraries( teststatus ${MOSQUITTO_LIBRARIES} ${CJSON_LIBRARIES} Threads::Threads rt )
  add_test( NAME teststatus COMMAND teststatus )

  # tellstick.conf read with libconfuse as telldusd reads it (if libconfuse is installed)
  find_path( CONFUSE_INCLUDE_DIR confuse.h )
  find_library( CONFUSE_LIBRARY confuse )
  set( CONFUSEDUMP "" )
  if( CONFUSE_INCLUDE_DIR AND CONFUSE_LIBRARY )
    add_executable( confusedump packaging/openwrt/tests/confusedump.c )
    target_include_directories( confusedump PRIVATE ${CONFUSE_INCLUDE_DIR} )
    target_link_libraries( confusedump ${CONFUSE_LIBRARY} )
    add_test( NAME tellstick-conf-libconfuse COMMAND confusedump ${CMAKE_CURRENT_SOURCE_DIR}/packaging/openwrt/tests/tellstick.conf )
    set( CONFUSEDUMP $<TARGET_FILE:confusedump> )
  endif()

  # OpenWrt scripts, skipped if ucode with uci is not installed
  add_test( NAME openwrt-scripts COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/packaging/openwrt/tests/run-tests.sh ${CONFUSEDUMP} )
  set_tests_properties( openwrt-scripts PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 )
  # uci-defaults and init script of packaging/openwrt/package, skipped as above or without uci
  add_test( NAME openwrt-package COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/packaging/openwrt/tests/test-package.sh )
  set_tests_properties( openwrt-package PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60 )
endif()
