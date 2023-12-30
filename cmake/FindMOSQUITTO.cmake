include( FindPackageHandleStandardArgs )

message( STATUS "Findmosquitto.cmake" )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list( APPEND CMAKE_PREFIX_PATH "/usr" )
  message(STATUS "Linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  list( APPEND CMAKE_PREFIX_PATH "C:/Program Files (x86)/mosquitto" )
  message(STATUS "Windows")
else()
  message(FATAL_ERROR "Only Linux and Windows targets currently supported!")
endif()

find_path(
  MOSQUITTO_INCLUDE_DIR
  mosquitto.h
  HINTS "${MOSQUITTO_DIR}/include"
)
message( STATUS "MOSQUITTO_INCLUDE_DIR=${MOSQUITTO_INCLUDE_DIR}" )

find_library( MOSQUITTO_LIBRARY
  NAME mosquitto 
  HINTS "${MOSQUITTO_DIR}/lib"
)
message( STATUS "MOSQUITTO_LIBRARY=${MOSQUITTO_LIBRARY}" )

find_package_handle_standard_args( MOSQUITTO DEFAULT_MSG
	MOSQUITTO_INCLUDE_DIR MOSQUITTO_LIBRARY
)

if( MOSQUITTO_FOUND )
  set( MOSQUITTO_INCLUDE_DIRS "${MOSQUITTO_INCLUDE_DIR}" )
  set( MOSQUITTO_LIBRARIES "${MOSQUITTO_LIBRARY}" )
else()
  message( STATUS "mosquitto NOT PACKAGES_FOUND" )
endif()
