# Hive Sources CMake File

set(HIVE_SOURCES_FOLDER ${CMAKE_CURRENT_SOURCE_DIR})

# Declare project
setup_project(Hive ${HIVE_VERSION} "${PROJECT_FULL_NAME}")

# Find dependencies
find_package(Qt5 COMPONENTS Widgets;Network REQUIRED)
if(ENABLE_HIVE_FEATURE_SPARKLE)
	find_package(Sparkle REQUIRED)
endif()

# Configure files based on CMakeLists.txt version number
configure_file(
	internals/config.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/internals/config.hpp
)

set(ADD_LINK_LIBS "")
set(PCH_HEADER_NAME "pch.hpp")

# Resource files
set(RESOURCE_FILES
	${HIVE_RESOURCES_FOLDER}/main.qrc
)
set(RESOURCE_FILES_GENERATED)

if(WIN32)
	configure_file(
		${HIVE_RESOURCES_FOLDER}/win32/Info.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/Info.rc
	)
	list(APPEND RESOURCE_FILES_GENERATED
		${CMAKE_CURRENT_BINARY_DIR}/Info.rc
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

set(HEADER_FILES_GENERATED
	${CMAKE_CURRENT_BINARY_DIR}/internals/config.hpp
)

set(HEADER_FILES_COMMON
	avdecc/mcDomainManager.hpp
	avdecc/controllerModel.hpp
	avdecc/channelConnectionManager.hpp
	avdecc/helper.hpp
	avdecc/mappingsHelper.hpp
	avdecc/hiveLogItems.hpp
	avdecc/loggerModel.hpp
	avdecc/commandChain.hpp
	avdecc/stringValidator.hpp
	connectionMatrix/cornerWidget.hpp
	connectionMatrix/headerView.hpp
	connectionMatrix/itemDelegate.hpp
	connectionMatrix/legendDialog.hpp
	connectionMatrix/model.hpp
	connectionMatrix/node.hpp
	connectionMatrix/paintHelper.hpp
	connectionMatrix/view.hpp
	counters/entityCountersTreeWidgetItem.hpp
	counters/avbInterfaceCountersTreeWidgetItem.hpp
	counters/clockDomainCountersTreeWidgetItem.hpp
	counters/streamInputCountersTreeWidgetItem.hpp
	counters/streamOutputCountersTreeWidgetItem.hpp
	mediaClock/mediaClockManagementDialog.hpp
	mediaClock/domainTreeModel.hpp
	mediaClock/unassignedListModel.hpp
	mediaClock/domainTreeDomainNameDelegate.hpp
	mediaClock/domainTreeEntityNameDelegate.hpp
	mediaClock/abstractTreeItem.hpp
	mediaClock/domainTreeItem.hpp
	mediaClock/entityTreeItem.hpp
	nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/controlValuesDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/discoveredInterfacesTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/memoryObjectDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/listenerStreamConnectionWidget.hpp
	nodeTreeDynamicWidgets/talkerStreamConnectionWidget.hpp
	nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/streamFormatComboBox.hpp
	nodeTreeDynamicWidgets/asPathWidget.hpp
	profiles/profiles.hpp
	profiles/profileSelectionDialog.hpp
	profiles/profileWidget.hpp
	settingsManager/settingsManager.hpp
	settingsManager/settingsSignaler.hpp
	settingsManager/settings.hpp
	sparkleHelper/sparkleHelper.hpp
	statistics/entityStatisticsTreeWidgetItem.hpp
	settingsDialog.hpp
	defaults.hpp
	deviceDetailsDialog.hpp
	deviceDetailsChannelTableModel.hpp
	aboutDialog.hpp
	loggerView.hpp
	firmwareUploadDialog.hpp
	multiFirmwareUpdateDialog.hpp
	mainWindow.hpp
	aecpCommandComboBox.hpp
	controlledEntityTreeWidget.hpp
	entityInspector.hpp
	mappingMatrix.hpp
	activeNetworkInterfacesModel.hpp
	networkInterfaceTypeModel.hpp
	nodeTreeWidget.hpp
	nodeVisitor.hpp
	windowsNpfHelper.hpp
)

set(SOURCE_FILES_COMMON
	avdecc/mcDomainManager.cpp
	avdecc/controllerModel.cpp
	avdecc/channelConnectionManager.cpp
	avdecc/helper.cpp
	avdecc/mappingsHelper.cpp
	avdecc/loggerModel.cpp
	avdecc/commandChain.cpp
	connectionMatrix/cornerWidget.cpp
	connectionMatrix/legendDialog.cpp
	connectionMatrix/headerView.cpp
	connectionMatrix/itemDelegate.cpp
	connectionMatrix/model.cpp
	connectionMatrix/node.cpp
	connectionMatrix/paintHelper.cpp
	connectionMatrix/view.cpp
	counters/entityCountersTreeWidgetItem.cpp
	counters/avbInterfaceCountersTreeWidgetItem.cpp
	counters/clockDomainCountersTreeWidgetItem.cpp
	counters/streamInputCountersTreeWidgetItem.cpp
	counters/streamOutputCountersTreeWidgetItem.cpp
	mediaClock/mediaClockManagementDialog.cpp
	mediaClock/domainTreeModel.cpp
	mediaClock/unassignedListModel.cpp
	mediaClock/domainTreeDomainNameDelegate.cpp
	mediaClock/domainTreeEntityNameDelegate.cpp
	mediaClock/abstractTreeItem.cpp
	mediaClock/domainTreeItem.cpp
	mediaClock/entityTreeItem.cpp
	nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/controlValuesDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/discoveredInterfacesTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/memoryObjectDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/listenerStreamConnectionWidget.cpp
	nodeTreeDynamicWidgets/talkerStreamConnectionWidget.cpp
	nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/streamFormatComboBox.cpp
	nodeTreeDynamicWidgets/asPathWidget.cpp
	profiles/profileSelectionDialog.cpp
	profiles/profileWidget.cpp
	settingsManager/settingsManager.cpp
	statistics/entityStatisticsTreeWidgetItem.cpp
	aboutDialog.cpp
	deviceDetailsDialog.cpp
	deviceDetailsChannelTableModel.cpp
	firmwareUploadDialog.cpp
	multiFirmwareUpdateDialog.cpp
	loggerView.cpp
	mainWindow.cpp
	settingsDialog.cpp
	aecpCommandComboBox.cpp
	controlledEntityTreeWidget.cpp
	entityInspector.cpp
	activeNetworkInterfacesModel.cpp
	networkInterfaceTypeModel.cpp
	nodeTreeWidget.cpp
	nodeVisitor.cpp
	windowsNpfHelper.cpp
)

set(UI_FILES_COMMON
	settingsDialog.ui
	mediaClock/mediaClockManagementDialog.ui
	mediaClock/domainTreeDomainNameDelegate.ui
	mediaClock/domainTreeEntityNameDelegate.ui
	deviceDetailsDialog.ui
	aboutDialog.ui
	loggerView.ui
	firmwareUploadDialog.ui
	multiFirmwareUpdateDialog.ui
	mainWindow.ui
)

set(SOURCE_FILES_APP
	main.cpp
)

if(NOT ENABLE_HIVE_FEATURE_SPARKLE)
	list(APPEND SOURCE_FILES_COMMON
		sparkleHelper/sparkleHelper_dummy.cpp
	)
elseif(WIN32)
	list(APPEND SOURCE_FILES_COMMON
		sparkleHelper/sparkleHelper_win32.cpp
	)
elseif(APPLE)
	list(APPEND SOURCE_FILES_COMMON
		sparkleHelper/sparkleHelper_mac.mm
	)
	list(APPEND ADD_LINK_LIBS "-framework Foundation")
else()
	list(APPEND SOURCE_FILES_COMMON
		sparkleHelper/sparkleHelper_unix.cpp
	)
endif()

# Group source files
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES_COMMON})
source_group("Header Files" FILES ${HEADER_FILES_GENERATED})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES_COMMON} ${SOURCE_FILES_APP})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "UI Files" FILES ${UI_FILES_COMMON})
source_group(TREE ${HIVE_RESOURCES_FOLDER} PREFIX "Resource Files" FILES ${RESOURCE_FILES})
source_group("Resource Files" FILES ${RESOURCE_FILES_GENERATED})

# Define static library to be used by main application and unit tests
add_library(${PROJECT_NAME}_static STATIC ${HEADER_FILES_COMMON} ${HEADER_FILES_GENERATED} ${SOURCE_FILES_COMMON} ${UI_FILES_COMMON})

# Enable precompiled headers
set_precompiled_headers(${PROJECT_NAME}_static ${PCH_HEADER_NAME})

# Setup common options
setup_library_options(${PROJECT_NAME}_static "${PROJECT_NAME}_static")

set_target_properties(${PROJECT_NAME}_static PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
)

# Additional compile definitions
target_compile_definitions(${PROJECT_NAME}_static PRIVATE RESOURCES_ROOT_DIR="${HIVE_RESOURCES_FOLDER}")

# Link libraries
target_link_libraries(${PROJECT_NAME}_static PUBLIC Qt5::Widgets Qt5::Network Hive_models_static QtMate_static Hive_widget_models_static la_avdecc_controller_cxx ${BUGREPORTER_LINK_LIBRARIES} libmarkdown)
if(ADD_LINK_LIBS)
	target_link_libraries(${PROJECT_NAME}_static PUBLIC ${ADD_LINK_LIBS})
endif()
if(ENABLE_HIVE_FEATURE_SPARKLE)
	target_compile_options(${PROJECT_NAME}_static PUBLIC -DUSE_SPARKLE)
	target_link_libraries(${PROJECT_NAME}_static PUBLIC Sparkle::lib)
endif()

# Include directories
target_include_directories(${PROJECT_NAME}_static
	PUBLIC
		$<BUILD_INTERFACE:${PROJECT_ROOT_DIR}/src>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# Application
if(BUILD_HIVE_APPLICATION)
	add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${HEADER_FILES_GENERATED} ${SOURCE_FILES_APP} ${RESOURCE_FILES} ${RESOURCE_FILES_GENERATED})

	# Setup common options
	setup_executable_options(${PROJECT_NAME})

	# Additional compile definitions
	if(HIVE_IS_RELEASE_VERSION)
		target_compile_definitions(${PROJECT_NAME} PRIVATE HIVE_IS_RELEASE_VERSION)
	endif()

	# Link libraries
	target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_static)

	set_target_properties(${PROJECT_NAME} PROPERTIES
		AUTORCC ON
	)

	# Configure Icon and PList files for macOS bundle
	if(APPLE)
		set_target_properties(${PROJECT_NAME} PROPERTIES
			MACOSX_BUNDLE_ICON_FILE ${ICON_FILE}
			MACOSX_BUNDLE_INFO_PLIST ${HIVE_RESOURCES_FOLDER}/macOS/Info.plist.in
		)
	endif()

	# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
	setup_deploy_runtime(${PROJECT_NAME} INSTALL ${SIGN_FLAG} ${SDR_PARAMETERS})
endif()

################
################
################
# Temporarily reduce warning level
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Clang and AppleClang
	target_compile_options(${PROJECT_NAME}_static PRIVATE -W -Wno-everything)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${PROJECT_NAME}_static PRIVATE -W -Wno-unused-variable -Wno-unused-but-set-variable -Wno-ignored-qualifiers -Wno-sign-compare -Wno-unused-parameter)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# Don't use Wall on MSVC, it prints too many stupid warnings
	target_compile_options(${PROJECT_NAME}_static PRIVATE /W3)
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
	message(FATAL_ERROR "Unsupported Compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

