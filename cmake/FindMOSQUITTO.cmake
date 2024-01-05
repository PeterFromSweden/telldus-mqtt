include( FindPackageHandleStandardArgs )

message( STATUS "FindMOSQUITTO.cmake" )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set( MOSQUITTO_DIR "/usr" )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set( MOSQUITTO_DIR "C:/Program Files/mosquitto" CACHE PATH "C:/Users/Peter/Downloads/mosquitto/out/install/x64-Debug" )
else()
  message(FATAL_ERROR "Only Linux and Windows targets currently supported!")
endif()
list( APPEND CMAKE_PREFIX_PATH "${TELLDUS_CORE_DIR}" )

find_path(
  MOSQUITTO_INCLUDE_DIR
  mosquitto.h
  HINTS "${MOSQUITTO_DIR}/include" "${MOSQUITTO_DIR}/devel"
)
message( STATUS "MOSQUITTO_INCLUDE_DIR=${MOSQUITTO_INCLUDE_DIR}" )

find_library( MOSQUITTO_LIBRARY
  NAME mosquitto 
  HINTS "${MOSQUITTO_DIR}/lib" "${MOSQUITTO_DIR}/devel"
)
message( STATUS "MOSQUITTO_LIBRARY=${MOSQUITTO_LIBRARY}" )

find_package_handle_standard_args( MOSQUITTO DEFAULT_MSG
	MOSQUITTO_INCLUDE_DIR MOSQUITTO_LIBRARY
)

if( MOSQUITTO_FOUND )
  set( MOSQUITTO_INCLUDE_DIRS "${MOSQUITTO_INCLUDE_DIR}" )
  set( MOSQUITTO_LIBRARIES "${MOSQUITTO_LIBRARY}" )
else()
  message( STATUS "MOSQUITTO NOT PACKAGES_FOUND" )
endif()
