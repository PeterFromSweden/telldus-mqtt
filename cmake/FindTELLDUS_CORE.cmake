include( FindPackageHandleStandardArgs )

message( STATUS "FindTELLDUS_CORE.cmake" )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set( TELLDUS_CORE_DIR "/usr" )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set( TELLDUS_CORE_DIR "C:/Program Files (x86)/Telldus" )
else()
  message(FATAL_ERROR "Only Linux and Windows targets currently supported!")
endif()
list( APPEND CMAKE_PREFIX_PATH "${TELLDUS_CORE_DIR}" )

find_path(
  TELLDUS_CORE_INCLUDE_DIR
  telldus-core.h
  HINTS "${TELLDUS_CORE_DIR}/include" "${TELLDUS_CORE_DIR}/Development"
)
message( STATUS "TELLDUS_CORE_INCLUDE_DIR=${TELLDUS_CORE_INCLUDE_DIR}" )

find_library( TELLDUS_CORE_LIBRARY
  NAMES telldus-core TelldusCore
  HINTS "${TELLDUS_CORE_DIR}/lib" "${TELLDUS_CORE_DIR}/Development/x86_64"
)
message( STATUS "TELLDUS_CORE_LIBRARY=${TELLDUS_CORE_LIBRARY}" )

find_package_handle_standard_args( TELLDUS_CORE DEFAULT_MSG
	TELLDUS_CORE_INCLUDE_DIR TELLDUS_CORE_LIBRARY
)

if( TELLDUS_CORE_FOUND )
  set( TELLDUS_CORE_INCLUDE_DIRS "${TELLDUS_CORE_INCLUDE_DIR}" )
  set( TELLDUS_CORE_LIBRARIES "${TELLDUS_CORE_LIBRARY}" )
else()
  message( STATUS "mosquitto NOT PACKAGES_FOUND" )
endif()
