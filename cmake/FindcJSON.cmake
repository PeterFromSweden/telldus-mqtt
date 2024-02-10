include( FindPackageHandleStandardArgs )

# Checks an environment variable; note that the first check
# does not require the usual CMake $-sign.
if( DEFINED ENV{CJSON_DIR} )
	set( CJSON_DIR "$ENV{CJSON_DIR}" )
endif()

find_path(
		CJSON_INCLUDE_DIR
		cjson/cJSON.h
	HINTS
		CJSON_DIR
)

find_library( CJSON_LIBRARY
	NAMES cjson
	HINTS ${CJSON_DIR}
)

if( MSVC )
	find_file( CJSON_DLL
		NAMES cjson.dll
		HINTS ${CJSON_LIBRARY}/../../bin
		)
	message( STATUS "CJSON_DLL=${CJSON_DLL}" )
	install( FILES ${CJSON_DLL} TYPE BIN )
endif()

FIND_PACKAGE_HANDLE_STANDARD_ARGS( cJSON DEFAULT_MSG
	CJSON_INCLUDE_DIR CJSON_LIBRARY
)

if( CJSON_FOUND )
	set( CJSON_INCLUDE_DIRS ${CJSON_INCLUDE_DIR} )
	set( CJSON_LIBRARIES ${CJSON_LIBRARY} )
	message( STATUS "CJSON_INCLUDE_DIRS=${CJSON_INCLUDE_DIRS}" )
	message( STATUS "CJSON_LIBRARIES=${CJSON_LIBRARIES}" )
	mark_as_advanced(
		CJSON_LIBRARY
		CJSON_INCLUDE_DIR
		CJSON_DIR
	)
else()
	set( CJSON_DIR "" CACHE STRING
		"An optional hint to a directory for finding `cJSON`"
	)
endif()
