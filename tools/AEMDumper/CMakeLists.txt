# Hive Sources CMake File

# Declare project
cu_setup_project(AEMDumper "1.0.0.1" "AEM Dumper")

# Find dependencies
find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Widgets;Network REQUIRED)

# Configure files based on CMakeLists.txt version number
configure_file(
	config.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/config.hpp
)

# Resource files
set(RESOURCE_FILES
	${HIVE_RESOURCES_FOLDER}/AEMDumper.qrc
)
set(RESOURCE_FILES_GENERATED)

if(WIN32)
	configure_file(
		${HIVE_RESOURCES_FOLDER}/win32/AEMDumper.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/AEMDumper.rc
	)
	list(APPEND RESOURCE_FILES_GENERATED
		${CMAKE_CURRENT_BINARY_DIR}/AEMDumper.rc
	)

	find_package(BugTrap REQUIRED)
	set(BUGREPORTER_LINK_LIBRARIES "BugTrap")
elseif(APPLE)
	set(ICON_FILE Icon.icns)
	set(ICON_PATH ${HIVE_RESOURCES_FOLDER}/macOS/${ICON_FILE})
	set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	list(APPEND RESOURCE_FILES
		${ICON_PATH}
	)
endif()

set(HEADER_FILES_SHARED
	${CU_ROOT_DIR}/src/avdecc/helper.hpp
)

set(SOURCE_FILES_SHARED
	${CU_ROOT_DIR}/src/avdecc/helper.cpp
)

set(HEADER_FILES_GENERATED
	${CMAKE_CURRENT_BINARY_DIR}/config.hpp
)

set(HEADER_FILES_COMMON
	mainWindow.hpp
)

set(SOURCE_FILES_COMMON
	mainWindow.cpp
)

set(UI_FILES_COMMON
	mainWindow.ui
)

set(SOURCE_FILES_APP
	main.cpp
)

# Group source files
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES_COMMON})
source_group("Header Files" FILES ${HEADER_FILES_GENERATED})
source_group("Header Files\\Shared" FILES ${HEADER_FILES_SHARED})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES_COMMON} ${SOURCE_FILES_APP})
source_group("Source Files\\Shared" FILES ${SOURCE_FILES_SHARED})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "UI Files" FILES ${UI_FILES_COMMON})
source_group(TREE ${HIVE_RESOURCES_FOLDER} PREFIX "Resource Files" FILES ${RESOURCE_FILES})
source_group("Resource Files" FILES ${RESOURCE_FILES_GENERATED})

# Application
add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${HEADER_FILES_SHARED} ${HEADER_FILES_COMMON} ${HEADER_FILES_GENERATED} ${SOURCE_FILES_SHARED} ${SOURCE_FILES_COMMON} ${SOURCE_FILES_APP} ${RESOURCE_FILES} ${RESOURCE_FILES_GENERATED} ${UI_FILES_COMMON})

# Setup common options
cu_setup_executable_options(${PROJECT_NAME})

# Additional compile definitions
target_compile_definitions(${PROJECT_NAME} PRIVATE RESOURCES_ROOT_DIR="${HIVE_RESOURCES_FOLDER}")

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_MAJOR_VERSION}::Widgets Hive_models_static QtMate_static Hive_widget_models_static ${BUGREPORTER_LINK_LIBRARIES})

# Include directories
target_include_directories(${PROJECT_NAME}
	PUBLIC
		$<BUILD_INTERFACE:${CU_ROOT_DIR}/src>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# Automatically generate MOC, UI and RCC
set_target_properties(${PROJECT_NAME} PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
	AUTORCC ON
)

# Configure Icon and PList files for macOS bundle
if(APPLE)
	set_target_properties(${PROJECT_NAME} PROPERTIES
		MACOSX_BUNDLE_ICON_FILE ${ICON_FILE}
		MACOSX_BUNDLE_INFO_PLIST ${HIVE_RESOURCES_FOLDER}/macOS/AEMDumper.plist.in
	)
endif()

# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
cu_setup_deploy_runtime(${PROJECT_NAME} INSTALL ${SIGN_FLAG} ${SDR_PARAMETERS})

################
################
################
# Temporarily reduce warning level
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Clang and AppleClang
	target_compile_options(${PROJECT_NAME} PRIVATE -W -Wno-everything)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${PROJECT_NAME} PRIVATE -W -Wno-unused-variable -Wno-unused-but-set-variable -Wno-ignored-qualifiers -Wno-sign-compare)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# Don't use Wall on MSVC, it prints too many stupid warnings
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
	message(FATAL_ERROR "Unsupported Compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

