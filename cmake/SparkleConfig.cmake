# CMake configuration file for Sparkle

if(WIN32)
	set(SPARKLE_BASE_DIR "${PROJECT_ROOT_DIR}/3rdparty/winsparkle")

	set(IMPORT_ARCH "x86")
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(IMPORT_ARCH "x64")
	endif()

	add_library(winsparkle SHARED IMPORTED GLOBAL)

	set_target_properties(winsparkle PROPERTIES
		IMPORTED_IMPLIB "${SPARKLE_BASE_DIR}/lib/${IMPORT_ARCH}/WinSparkle.lib"
		IMPORTED_LOCATION "${SPARKLE_BASE_DIR}/bin/${IMPORT_ARCH}/WinSparkle.dll"
		INTERFACE_INCLUDE_DIRECTORIES "${SPARKLE_BASE_DIR}/include"
	)

	add_library(Sparkle::lib ALIAS winsparkle)

elseif(APPLE)
	set(SPARKLE_BASE_DIR "${PROJECT_ROOT_DIR}/3rdparty/sparkle")
	set_source_files_properties(${SPARKLE_BASE_DIR}/Sparkle.framework PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks) # MACOSX_PACKAGE_LOCATION = Place a 'source file' (here it is Sparkle.framework) into the app bundle

	add_library(sparkle SHARED IMPORTED GLOBAL)

	set_target_properties(sparkle PROPERTIES
		IMPORTED_LOCATION "${SPARKLE_BASE_DIR}/Sparkle.framework/Sparkle"
		INTERFACE_SOURCES "${SPARKLE_BASE_DIR}/Sparkle.framework" # So that our framework is considered a source file, and copied to the app bundle through MACOSX_PACKAGE_LOCATION
		INTERFACE_LINK_LIBRARIES "${SPARKLE_BASE_DIR}/Sparkle.framework" # So the framework is linked to the target, as well as adding include search path (automatically done by cmake when detecting a framework)
	)

	add_library(Sparkle::lib ALIAS sparkle)

else()
	add_library(dummySparkle INTERFACE IMPORTED GLOBAL)
	add_library(Sparkle::lib ALIAS dummySparkle)

endif()
