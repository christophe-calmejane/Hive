# CMake import target configuration file

if(WIN32)
	set(SPARKLE_BASE_DIR "${PROJECT_ROOT_DIR}/3rdparty/winsparkle")

	set(IMPORT_ARCH "x86")
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(IMPORT_ARCH "x64")
	endif()

	add_library(sparkle SHARED IMPORTED)

	set_target_properties(sparkle PROPERTIES
		IMPORTED_IMPLIB "${SPARKLE_BASE_DIR}/lib/${IMPORT_ARCH}/WinSparkle.lib"
		IMPORTED_LOCATION "${SPARKLE_BASE_DIR}/bin/${IMPORT_ARCH}/WinSparkle.dll"
		INTERFACE_INCLUDE_DIRECTORIES "${SPARKLE_BASE_DIR}/include"
	)

	# add_library(Sparkle::lib ALIAS winsparkle)

elseif(APPLE)
	message(FATAL_ERROR "TODO: Sparkle for macOS")
else()
	message(FATAL_ERROR "TODO: Sparkle for unix")
endif()
