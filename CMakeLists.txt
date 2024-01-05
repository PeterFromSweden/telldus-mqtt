cmake_minimum_required( VERSION 3.1 )

project( telldus-core-mqtt VERSION 0.0.2 LANGUAGES C )
set( CMAKE_C_STANDARD 11 )

# package find location
list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/" )
find_package(MOSQUITTO REQUIRED)
find_package(TELLDUS_CORE REQUIRED)
find_package(cJSON REQUIRED)

add_executable( ${PROJECT_NAME}
  "src/main.c"  "src/config.c" "src/config.h" "src/criticalsection.h" "src/criticalsection_stub.c" "src/mythread_win.c")

target_include_directories( ${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})
target_link_libraries( ${PROJECT_NAME} ${MOSQUITTO_LIBRARIES} ${TELLDUS_CORE_LIBRARIES}  ${CJSON_LIBRARIES})

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/telldus-core-mqtt.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

install( TARGETS ${PROJECT_NAME} )

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()
