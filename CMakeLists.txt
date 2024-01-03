cmake_minimum_required( VERSION 3.1 )

project( telldus-core-mqtt VERSION 0.0.2 LANGUAGES C )

# package find location
list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/" )
find_package(MOSQUITTO REQUIRED)
find_package(TELLDUS_CORE REQUIRED)
find_package(cJSON REQUIRED)

add_executable( ${PROJECT_NAME}
  "src/main.c"  "src/config.c" "src/config.h")

target_include_directories( ${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS} ${TELLDUS_CORE_INCLUDE_DIRS} ${CJSON_INCLUDE_DIRS})
target_link_libraries( ${PROJECT_NAME} ${MOSQUITTO_LIBRARIES} ${TELLDUS_CORE_LIBRARIES}  ${CJSON_LIBRARIES})

install( TARGETS ${PROJECT_NAME} )
