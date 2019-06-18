# Hive Application CMake File

# Declare project
setup_project(Hive ${HIVE_VERSION} "${PROJECT_FULL_NAME}")

# Find Qt dependencies
find_package(Qt5 COMPONENTS Widgets;Network REQUIRED)

# Configure files based on CMakeLists.txt version number
configure_file(
	internals/config.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/internals/config.hpp
)

# Resource files
set(RESOURCES_FOLDER "${PROJECT_ROOT_DIR}/resources")
set(RESOURCE_FILES
	${RESOURCES_FOLDER}/main.qrc
)
set(RESOURCE_FILES_GENERATED)

if(WIN32)
	configure_file(
		${RESOURCES_FOLDER}/win32/Info.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/Info.rc
	)
	list(APPEND RESOURCE_FILES_GENERATED
		${CMAKE_CURRENT_BINARY_DIR}/Info.rc
	)
	
	find_package(BugTrap REQUIRED)
	set(BUGREPORTER_LINK_LIBRARIES "BugTrap")
elseif(APPLE)
	set(ICON_FILE Icon.icns)
	set(ICON_PATH ${RESOURCES_FOLDER}/macOS/${ICON_FILE})
	set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	list(APPEND RESOURCE_FILES
		${ICON_PATH}
	)
endif()

set(HEADER_FILES_GENERATED
	${CMAKE_CURRENT_BINARY_DIR}/internals/config.hpp
)

set(HEADER_FILES_COMMON
	avdecc/controllerManager.hpp
	avdecc/mcDomainManager.hpp
	avdecc/controllerModel.hpp
	avdecc/channelConnectionManager.hpp
	avdecc/helper.hpp
	avdecc/hiveLogItems.hpp
	avdecc/loggerModel.hpp
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
	settingsManager/settingsManager.hpp
	settingsManager/settings.hpp
	statistics/entityStatisticsTreeWidgetItem.hpp
	toolkit/comboBox.hpp
	toolkit/dynamicHeaderView.hpp
	toolkit/material/button.hpp
	toolkit/material/color.hpp
	toolkit/material/colorPalette.hpp
	toolkit/material/helper.hpp
	toolkit/tableView.hpp
	toolkit/textEntry.hpp
	toolkit/tickableMenu.hpp
	toolkit/graph/connection.hpp
	toolkit/graph/inputSocket.hpp
	toolkit/graph/node.hpp
	toolkit/graph/outputSocket.hpp
	toolkit/graph/socket.hpp
	toolkit/graph/view.hpp
	updater/updater.hpp
	settingsDialog.hpp
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
	entityLogoCache.hpp
	highlightForegroundItemDelegate.hpp
	imageItemDelegate.hpp
	mappingMatrix.hpp
	activeNetworkInterfaceModel.hpp
	networkInterfaceModel.hpp
	networkInterfaceTypeModel.hpp
	nodeTreeWidget.hpp
	nodeVisitor.hpp
	painterHelper.hpp
)

set(SOURCE_FILES_COMMON
	avdecc/controllerManager.cpp
	avdecc/mcDomainManager.cpp
	avdecc/controllerModel.cpp
	avdecc/channelConnectionManager.cpp
	avdecc/helper.cpp
	avdecc/loggerModel.cpp
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
	toolkit/comboBox.cpp
	toolkit/dynamicHeaderView.cpp
	toolkit/material/button.cpp
	toolkit/material/color.cpp
	toolkit/material/colorPalette.cpp
	toolkit/material/helper.cpp
	toolkit/tableView.cpp
	toolkit/textEntry.cpp
	toolkit/tickableMenu.cpp
	toolkit/graph/connection.cpp
	toolkit/graph/inputSocket.cpp
	toolkit/graph/node.cpp
	toolkit/graph/outputSocket.cpp
	toolkit/graph/socket.cpp
	toolkit/graph/view.cpp
	updater/updater.cpp
	aboutDialog.cpp
	deviceDetailsDialog.cpp
	deviceDetailsChannelTableModel.cpp
	firmwareUploadDialog.cpp
	multiFirmwareUpdateDialog.cpp
	loggerView.cpp
	mainWindow.cpp
	settingsDialog.cpp
	aecpCommandComboBox.cpp
	entityLogoCache.cpp
	highlightForegroundItemDelegate.cpp
	imageItemDelegate.cpp
	controlledEntityTreeWidget.cpp
	entityInspector.cpp
	activeNetworkInterfaceModel.cpp
	networkInterfaceModel.cpp
	networkInterfaceTypeModel.cpp
	nodeTreeWidget.cpp
	nodeVisitor.cpp
	painterHelper.cpp
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

# Group source files
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES_COMMON})
source_group("Header Files" FILES ${HEADER_FILES_GENERATED})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES_COMMON} ${SOURCE_FILES_APP})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "UI Files" FILES ${UI_FILES_COMMON})
source_group(TREE ${RESOURCES_FOLDER} PREFIX "Resource Files" FILES ${RESOURCE_FILES})
source_group("Resource Files" FILES ${RESOURCE_FILES_GENERATED})

# Define static library to be used by main application and unit tests
add_library(${PROJECT_NAME}_static STATIC ${HEADER_FILES_COMMON} ${HEADER_FILES_GENERATED} ${SOURCE_FILES_COMMON} ${UI_FILES_COMMON})

# Setup common options
setup_library_options(${PROJECT_NAME}_static "${PROJECT_NAME}_static")

set_target_properties(${PROJECT_NAME}_static PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
)

# Additional compile definitions
target_compile_definitions(${PROJECT_NAME}_static PRIVATE RESOURCES_ROOT_DIR="${RESOURCES_FOLDER}")

# Link libraries
target_link_libraries(${PROJECT_NAME}_static PUBLIC Qt5::Widgets Qt5::Network la_avdecc_controller_cxx ${BUGREPORTER_LINK_LIBRARIES} libmarkdown)

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

	# Link libraries
	target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_NAME}_static)

	set_target_properties(${PROJECT_NAME} PROPERTIES
		AUTORCC ON
	)

	# Configure Icon and PList files for macOS bundle
	if(APPLE)
		set_target_properties(${PROJECT_NAME} PROPERTIES
			MACOSX_BUNDLE_ICON_FILE ${ICON_FILE}
			MACOSX_BUNDLE_INFO_PLIST ${RESOURCES_FOLDER}/macOS/Info.plist.in
		)
	endif()

	# Deploy required shared libraries and resources to the output folder and setup installation rules (must be done BEFORE sign_target or qtDeploy will strip the signature from the binary)
	target_setup_deploy(${PROJECT_NAME} INSTALL)

	# Sign binary (this is done during installation phase)
	if(ENABLE_HIVE_SIGNING)
		sign_target(${PROJECT_NAME})
	endif()

	# Set installation rule (always installing application)
	install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION . RUNTIME DESTINATION bin)
endif()

################
################
################
# Temporarily reduce warning level
if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX OR APPLE OR VS_USE_CLANG)
	target_compile_options(${PROJECT_NAME}_static PRIVATE -W -Wno-everything)
elseif(MSVC)
	# Don't use Wall on MSVC, it prints too many stupid warnings
	target_compile_options(${PROJECT_NAME}_static PRIVATE /W3)
endif()
