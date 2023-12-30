include( FindPackageHandleStandardArgs )

message( STATUS "FindTELLDUS_CORE.cmake" )

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  list( APPEND CMAKE_PREFIX_PATH "/usr" )
  message(STATUS "Linux")
else()
  message(FATAL_ERROR "Only Linux  targets currently supported!")
endif()

find_path(
  TELLDUS_CORE_INCLUDE_DIR
  telldus-core.h
  HINTS "${TELLDUS_CORE_DIR}/include"
)
message( STATUS "TELLDUS_CORE_INCLUDE_DIR=${TELLDUS_CORE_INCLUDE_DIR}" )

find_library( TELLDUS_CORE_LIBRARY
  NAME telldus-core 
  HINTS "${TELLDUS_CORE_DIR}/lib"
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
