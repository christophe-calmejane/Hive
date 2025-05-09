# Hive Sources CMake File

# Declare project
cu_setup_project(Hive ${HIVE_VERSION} "${PROJECT_FULL_NAME}" MARKETING_VERSION_DIGITS ${MARKETING_VERSION_DIGITS} MARKETING_VERSION_POSTFIX ${MARKETING_VERSION_POSTFIX})

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

set(CONFIGURE_IN_FILES
	internals/config.hpp.in
)

if(WIN32)
	configure_file(
		${HIVE_RESOURCES_FOLDER}/win32/Info.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/Info.rc
	)
	list(APPEND RESOURCE_FILES_GENERATED
		${CMAKE_CURRENT_BINARY_DIR}/Info.rc
	)
	list(APPEND CONFIGURE_IN_FILES
		${HIVE_RESOURCES_FOLDER}/win32/Info.rc.in
	)
	
	find_package(BugTrap REQUIRED)
	set(BUGREPORTER_LINK_LIBRARIES "BugTrap")
elseif(APPLE)
	# Main icon
	set(ICON_FILE Icon.icns)
	set(ICON_PATH ${HIVE_RESOURCES_FOLDER}/macOS/${ICON_FILE})
	set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	list(APPEND RESOURCE_FILES
		${ICON_PATH}
	)
	# Add other icons
	set_source_files_properties(${HIVE_RESOURCES_FOLDER}/macOS/Icon_AVE.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	set_source_files_properties(${HIVE_RESOURCES_FOLDER}/macOS/Icon_ANS.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	list(APPEND RESOURCE_FILES
		${HIVE_RESOURCES_FOLDER}/macOS/Icon_AVE.icns
		${HIVE_RESOURCES_FOLDER}/macOS/Icon_ANS.icns
	)
endif()

set(HEADER_FILES_GENERATED
	${CMAKE_CURRENT_BINARY_DIR}/internals/config.hpp
)

set(HEADER_FILES_COMMON
	avdecc/mcDomainManager.hpp
	avdecc/channelConnectionManager.hpp
	avdecc/helper.hpp
	avdecc/mappingsHelper.hpp
	avdecc/hiveLogItems.hpp
	avdecc/loggerModel.hpp
	avdecc/commandChain.hpp
	avdecc/stringValidator.hpp
	avdecc/euiValidator.hpp
	avdecc/numberValidator.hpp
	connectionEditor/connectionEditor.hpp
	connectionEditor/connectionWorkspace.hpp
	connectionEditor/nodeListModel.hpp
	connectionEditor/nodeListView.hpp
	connectionEditor/nodeOrganizer.hpp
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
	discoveredEntities/view.hpp
	diagnostics/controlDiagnosticsTreeWidgetItem.hpp
	diagnostics/entityDiagnosticsTreeWidgetItem.hpp
	diagnostics/streamInputDiagnosticsTreeWidgetItem.hpp
	mediaClock/mediaClockManagementDialog.hpp
	mediaClock/domainTreeModel.hpp
	mediaClock/unassignedListModel.hpp
	mediaClock/domainTreeDomainNameDelegate.hpp
	mediaClock/domainTreeEntityNameDelegate.hpp
	mediaClock/abstractTreeItem.hpp
	mediaClock/domainTreeItem.hpp
	mediaClock/entityTreeItem.hpp
	newsFeed/newsFeed.hpp
	nodeTreeDynamicWidgets/milanDynamicStateTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/controlValuesDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/discoveredInterfacesTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/memoryObjectDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/listenerStreamConnectionWidget.hpp
	nodeTreeDynamicWidgets/talkerStreamConnectionWidget.hpp
	nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/asPathWidget.hpp
	processHelper/processHelper.hpp
	profiles/profiles.hpp
	profiles/profileSelectionDialog.hpp
	profiles/profileWidget.hpp
	settingsManager/settingsManager.hpp
	settingsManager/settingsSignaler.hpp
	settingsManager/settings.hpp
	statistics/entityStatisticsTreeWidgetItem.hpp
	settingsDialog.hpp
	defaults.hpp
	deviceDetailsDialog.hpp
	deviceDetailsChannelTableModel.hpp
	deviceDetailsStreamFormatTableModel.hpp
	deviceDetailsLatencyTableModel.hpp
	aboutDialog.hpp
	loggerView.hpp
	discoveredEntitiesView.hpp
	firmwareUploadDialog.hpp
	multiFirmwareUpdateDialog.hpp
	mainWindow.hpp
	aecpCommandComboBox.hpp
	aecpCommandSlider.hpp
	aecpCommandSpinBox.hpp
	aecpCommandTextEntry.hpp
	controlledEntityTreeWidget.hpp
	controlledEntityTreeWidgetItemDelegate.hpp
	entityInspector.hpp
	entityInspectorRoles.hpp
	mappingMatrix.hpp
	activeNetworkInterfacesModel.hpp
	latencyItemDelegate.hpp
	networkInterfaceTypeModel.hpp
	nodeTreeWidget.hpp
	nodeDispatcher.hpp
	windowsNpfHelper.hpp
	streamFormatComboBox.hpp
	latencyComboBox.hpp
	visibilitySettings.hpp
	listViewMatrixViewController.hpp
	application.hpp
)

set(SOURCE_FILES_COMMON
	avdecc/mcDomainManager.cpp
	avdecc/channelConnectionManager.cpp
	avdecc/helper.cpp
	avdecc/mappingsHelper.cpp
	avdecc/loggerModel.cpp
	avdecc/commandChain.cpp
	connectionEditor/connectionEditor.cpp
	connectionEditor/connectionWorkspace.cpp
	connectionEditor/nodeListModel.cpp
	connectionEditor/nodeListView.cpp
	connectionEditor/nodeOrganizer.cpp
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
	discoveredEntities/view.cpp
	diagnostics/controlDiagnosticsTreeWidgetItem.cpp
	diagnostics/entityDiagnosticsTreeWidgetItem.cpp
	diagnostics/streamInputDiagnosticsTreeWidgetItem.cpp
	mediaClock/mediaClockManagementDialog.cpp
	mediaClock/domainTreeModel.cpp
	mediaClock/unassignedListModel.cpp
	mediaClock/domainTreeDomainNameDelegate.cpp
	mediaClock/domainTreeEntityNameDelegate.cpp
	mediaClock/abstractTreeItem.cpp
	mediaClock/domainTreeItem.cpp
	mediaClock/entityTreeItem.cpp
	newsFeed/newsFeed.cpp
	nodeTreeDynamicWidgets/milanDynamicStateTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/controlValuesDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/discoveredInterfacesTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/memoryObjectDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/listenerStreamConnectionWidget.cpp
	nodeTreeDynamicWidgets/talkerStreamConnectionWidget.cpp
	nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/asPathWidget.cpp
	profiles/profileSelectionDialog.cpp
	profiles/profileWidget.cpp
	settingsManager/settingsManager.cpp
	statistics/entityStatisticsTreeWidgetItem.cpp
	aboutDialog.cpp
	deviceDetailsDialog.cpp
	deviceDetailsChannelTableModel.cpp
	deviceDetailsStreamFormatTableModel.cpp
	deviceDetailsLatencyTableModel.cpp
	firmwareUploadDialog.cpp
	multiFirmwareUpdateDialog.cpp
	loggerView.cpp
	discoveredEntitiesView.cpp
	mainWindow.cpp
	settingsDialog.cpp
	controlledEntityTreeWidget.cpp
	controlledEntityTreeWidgetItemDelegate.cpp
	entityInspector.cpp
	mappingMatrix.cpp
	activeNetworkInterfacesModel.cpp
	latencyItemDelegate.cpp
	networkInterfaceTypeModel.cpp
	nodeTreeWidget.cpp
	nodeDispatcher.cpp
	windowsNpfHelper.cpp
	listViewMatrixViewController.cpp
	application.cpp
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

# OS-specific files and options
set(OSDEP_SOURCE_FILES "")
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" OR CMAKE_SYSTEM_NAME STREQUAL "iOS")
	list(APPEND OSDEP_SOURCE_FILES processHelper/processHelper_macos.mm)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
	list(APPEND OSDEP_SOURCE_FILES processHelper/processHelper_win32.cpp)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android" OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
	list(APPEND OSDEP_SOURCE_FILES processHelper/processHelper_unix.cpp)
else()
	message(FATAL_ERROR "Unsupported system: ${CMAKE_SYSTEM_NAME}")
endif()

# Group source files
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES_COMMON})
source_group("Header Files\\\\Generated" FILES ${HEADER_FILES_GENERATED})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES_COMMON} ${SOURCE_FILES_APP})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files\\OS-Dep Files" FILES ${OSDEP_SOURCE_FILES})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "UI Files" FILES ${UI_FILES_COMMON})
source_group(TREE ${HIVE_RESOURCES_FOLDER} PREFIX "Resource Files" FILES ${RESOURCE_FILES})
source_group("Resource Files\\\\Generated" FILES ${RESOURCE_FILES_GENERATED})
source_group("Configure Input Files" FILES ${CONFIGURE_IN_FILES})

# Define static library to be used by main application and unit tests
add_library(${PROJECT_NAME}_static STATIC ${HEADER_FILES_COMMON} ${HEADER_FILES_GENERATED} ${SOURCE_FILES_COMMON} ${OSDEP_SOURCE_FILES} ${UI_FILES_COMMON} ${CONFIGURE_IN_FILES})

# Setup common options
cu_setup_library_options(${PROJECT_NAME}_static)

# Automatically generate MOC and UI
set_target_properties(${PROJECT_NAME}_static PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
)

# Enable precompiled headers
cu_set_precompiled_headers(${PROJECT_NAME}_static ${PCH_HEADER_NAME})

# Additional compile definitions
target_compile_definitions(${PROJECT_NAME}_static PRIVATE RESOURCES_ROOT_DIR="${HIVE_RESOURCES_FOLDER}")

# Link libraries
target_link_libraries(${PROJECT_NAME}_static PUBLIC Qt${QT_MAJOR_VERSION}::Widgets Qt${QT_MAJOR_VERSION}::Network Hive_models_static QtMate_static Hive_widget_models_static la_avdecc_controller_cxx ${BUGREPORTER_LINK_LIBRARIES} libmarkdown)
if(ADD_LINK_LIBS)
	target_link_libraries(${PROJECT_NAME}_static PUBLIC ${ADD_LINK_LIBS})
endif()
if(ENABLE_HIVE_FEATURE_SPARKLE)
	target_compile_definitions(${PROJECT_NAME}_static PUBLIC USE_SPARKLE)
	# Link with SparkleHelper
	fixup_sparkleHelper_dependencies() # Temporary fix to resolve SparkleHelper transitive dependencies
	target_link_libraries(${PROJECT_NAME}_static PUBLIC SparkleHelper::lib)
endif()

# Include directories
target_include_directories(${PROJECT_NAME}_static
	PUBLIC
		$<BUILD_INTERFACE:${CU_ROOT_DIR}/src>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

# Application
if(BUILD_HIVE_APPLICATION)
	add_executable(${PROJECT_NAME} WIN32 MACOSX_BUNDLE ${HEADER_FILES_GENERATED} ${SOURCE_FILES_APP} ${RESOURCE_FILES} ${RESOURCE_FILES_GENERATED})

	# Setup common options
	cu_setup_executable_options(${PROJECT_NAME})

	# Additional compile definitions
	if(HIVE_IS_RELEASE_VERSION)
		target_compile_definitions(${PROJECT_NAME} PRIVATE HIVE_IS_RELEASE_VERSION)
	endif()

	# Link libraries
	target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_static)

	# Set auto RCC
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

	if(ENABLE_HIVE_FEATURE_SPARKLE)
		fixup_sparkleHelper_resources(${PROJECT_NAME}) # Temporary fix to resolve SparkleHelper resources
	endif()

	# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
	cu_setup_deploy_runtime(${PROJECT_NAME} INSTALL ${SIGN_FLAG} ${SDR_PARAMETERS})
endif()

################
################
################
# Temporarily reduce warning level
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Clang and AppleClang
	target_compile_options(${PROJECT_NAME}_static PRIVATE -W -Wno-everything)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${PROJECT_NAME}_static PRIVATE -W -Wno-unused-variable -Wno-unused-but-set-variable -Wno-ignored-qualifiers -Wno-sign-compare -Wno-unused-parameter -Wno-maybe-uninitialized)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# Don't use Wall on MSVC, it prints too many stupid warnings
	target_compile_options(${PROJECT_NAME}_static PRIVATE /W3)
	target_compile_options(${PROJECT_NAME} PRIVATE /W3)
else()
	message(FATAL_ERROR "Unsupported Compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

