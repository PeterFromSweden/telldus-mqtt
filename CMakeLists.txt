cmake_minimum_required( VERSION 3.1 )

project( TelldusCoreMQTT VERSION 0.0.1 LANGUAGES C )

# package find location
list( APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/" )
find_package(MOSQUITTO REQUIRED)

add_executable( ${PROJECT_NAME}
  "src/main.c" )

target_include_directories( ${PROJECT_NAME} PRIVATE ${MOSQUITTO_INCLUDE_DIRS} )
target_link_libraries( ${PROJECT_NAME} ${MOSQUITTO_LIBRARIES} )

#file(COPY ${SOME_VAR}
#    DESTINATION ${EXECUTABLE_OUTPUT_PATH})