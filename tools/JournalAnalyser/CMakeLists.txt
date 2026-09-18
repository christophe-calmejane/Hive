# Journal Analyser CMake File

# Declare project (the full name is scoped to this directory, so it does not override the one of the main application)
set(CU_PROJECT_FULL_NAME "Journal Analyser")
cu_setup_project(JournalAnalyser ${JOURNALANALYSER_VERSION} "Journal Analyser - Offline analysis of Hive Event Journal files" MARKETING_VERSION_DIGITS ${MARKETING_VERSION_DIGITS} MARKETING_VERSION_POSTFIX ${MARKETING_VERSION_POSTFIX})

# Find dependencies
find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Widgets REQUIRED)

# Configure files based on CMakeLists.txt version number
configure_file(
	config.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/config.hpp
)

# Resource files (the Hive resources are required by the reused Hive widgets: fonts, stylesheet, icons)
set(RESOURCE_FILES
	${HIVE_RESOURCES_FOLDER}/main.qrc
)
set(RESOURCE_FILES_GENERATED)

if(WIN32)
	configure_file(
		${HIVE_RESOURCES_FOLDER}/win32/JournalAnalyser.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/JournalAnalyser.rc
	)
	list(APPEND RESOURCE_FILES_GENERATED
		${CMAKE_CURRENT_BINARY_DIR}/JournalAnalyser.rc
	)
elseif(APPLE)
	# The journal icon is both the application icon and the document icon
	set(ICON_FILE Icon_HEJ.icns)
	set(ICON_PATH ${HIVE_RESOURCES_FOLDER}/macOS/${ICON_FILE})
	set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	list(APPEND RESOURCE_FILES
		${ICON_PATH}
	)
endif()

set(HEADER_FILES_GENERATED
	${CMAKE_CURRENT_BINARY_DIR}/config.hpp
)

set(HEADER_FILES_COMMON
	analyserWindow.hpp
)

set(SOURCE_FILES_COMMON
	analyserWindow.cpp
)

set(SOURCE_FILES_APP
	main.cpp
)

# Group source files
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES_COMMON})
source_group("Header Files" FILES ${HEADER_FILES_GENERATED})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES_COMMON} ${SOURCE_FILES_APP})
source_group(TREE ${HIVE_RESOURCES_FOLDER} PREFIX "Resource Files" FILES ${RESOURCE_FILES})
source_group("Resource Files" FILES ${RESOURCE_FILES_GENERATED})

# Application
add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${HEADER_FILES_COMMON} ${HEADER_FILES_GENERATED} ${SOURCE_FILES_COMMON} ${SOURCE_FILES_APP} ${RESOURCE_FILES} ${RESOURCE_FILES_GENERATED})

# Setup common options
cu_setup_executable_options(${PROJECT_NAME})

if(ENABLE_HIVE_FEATURE_SPARKLE)
	fixup_sparkleHelper_dependencies() # Temporary fix to resolve SparkleHelper transitive dependencies
endif()

# Link libraries (the Hive static library provides the Event Journal viewer, the Entity Inspector, the Network Graph and the application/settings plumbing)
target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_MAJOR_VERSION}::Widgets Hive_static)

# Include directories
target_include_directories(${PROJECT_NAME}
	PRIVATE
		${CMAKE_CURRENT_BINARY_DIR}
)

# Automatically generate MOC and RCC
set_target_properties(${PROJECT_NAME} PROPERTIES
	AUTOMOC ON
	AUTORCC ON
)

# Set IDE folder
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "Tools")

# Configure Icon and PList files for macOS bundle
if(APPLE)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		MACOSX_BUNDLE_ICON_FILE ${ICON_FILE}
		MACOSX_BUNDLE_INFO_PLIST ${HIVE_RESOURCES_FOLDER}/macOS/JournalAnalyser.plist.in
	)
endif()

# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
cu_setup_deploy_runtime(${PROJECT_NAME} INSTALL ${SIGN_FLAG} ${SDR_PARAMETERS})

################
################
################
# Temporarily reduce warning level (the reused Hive headers are compiled with the reduced level of the Hive static library)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Clang and AppleClang
	target_compile_options(${PROJECT_NAME} PRIVATE -W -Wno-everything)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${PROJECT_NAME} PRIVATE -W -Wno-unused-variable -Wno-unused-but-set-variable -Wno-ignored-qualifiers -Wno-sign-compare -Wno-unused-parameter -Wno-maybe-uninitialized)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# Don't use Wall on MSVC, it prints too many stupid warnings
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
	message(FATAL_ERROR "Unsupported Compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()
