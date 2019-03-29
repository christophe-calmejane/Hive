# Main project

# Declare project
project(${PROJECT_NAME} LANGUAGES C CXX VERSION ${HIVE_VERSION})

############ Define some variables (most to be removed when reworking cmake files and using setup_project macro)
if(NOT PROJECT_VERSION_TWEAK)
	set(PROJECT_VERSION_TWEAK 0)
	set(LA_PROJECT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
	set(LA_PROJECT_CMAKEVERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
else()
	set(LA_PROJECT_VERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}-beta${PROJECT_VERSION_TWEAK}")
	set(LA_PROJECT_CMAKEVERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_TWEAK}")
endif()
set(LA_PROJECT_PRODUCTVERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}")
set(LA_PROJECT_FILEVERSION_STRING "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${PROJECT_VERSION_TWEAK}")


# Declare executable target
add_executable(${PROJECT_NAME} MACOSX_BUNDLE WIN32 "")

# Setup debug symbols
setup_symbols(${PROJECT_NAME})

# Set the "DEBUG" define in debug compilation mode
set_debug_define(${PROJECT_NAME})

# Add a postfix in debug mode
set_target_properties(${PROJECT_NAME} PROPERTIES DEBUG_POSTFIX "-d")

# Configure file based on CMakeLists.txt
configure_file(
	internals/config.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/internals/config.hpp
)

target_compile_definitions(${PROJECT_NAME} PRIVATE RESOURCES_ROOT_DIR="${PROJECT_ROOT_DIR}/resources")

# Process Qt resources
qt5_add_resources(
	RCC_RESOURCES
	${PROJECT_ROOT_DIR}/resources/main.qrc)

# Resource files
set(RESOURCE_FILES
	${RCC_RESOURCES}
	${CMAKE_CURRENT_BINARY_DIR}/internals/config.hpp
)

if(WIN32)
	set(ICON_FILE Icon.ico)
	set(ICON_PATH ${PROJECT_ROOT_DIR}/resources/win32/${ICON_FILE})
	configure_file(
		${PROJECT_ROOT_DIR}/resources/win32/Info.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/Info.rc
	)
	list(APPEND RESOURCE_FILES
		${CMAKE_CURRENT_BINARY_DIR}/Info.rc
	)
	
	find_package(BugTrap REQUIRED)
	set(BUGREPORTER_LINK_LIBRARIES "BugTrap")
elseif(APPLE)
	set(ICON_FILE Icon.icns)
	set(ICON_PATH ${PROJECT_ROOT_DIR}/resources/macOS/${ICON_FILE})
	set_target_properties(${PROJECT_NAME} PROPERTIES
		MACOSX_BUNDLE_ICON_FILE ${ICON_FILE}
		MACOSX_BUNDLE_INFO_PLIST ${PROJECT_ROOT_DIR}/resources/macOS/Info.plist.in
	)
	set_source_files_properties(${ICON_PATH} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	list(APPEND RESOURCE_FILES
		${ICON_PATH}
	)
endif()

# Toolkit header files
set(TOOLKIT_HEADER_FILES
	toolkit/comboBox.hpp
	toolkit/dynamicHeaderView.hpp
	toolkit/materialButton.hpp
	toolkit/materialPalette.hpp
	toolkit/tableView.hpp
	toolkit/textEntry.hpp
	toolkit/tickableMenu.hpp
	toolkit/transposeProxyModel.hpp
	toolkit/graph/connection.hpp
	toolkit/graph/inputSocket.hpp
	toolkit/graph/node.hpp
	toolkit/graph/outputSocket.hpp
	toolkit/graph/socket.hpp
	toolkit/graph/view.hpp
)

# Toolkit source files
set(TOOLKIT_SOURCE_FILES
	toolkit/comboBox.cpp
	toolkit/dynamicHeaderView.cpp
	toolkit/materialButton.cpp
	toolkit/materialPalette.cpp
	toolkit/tableView.cpp
	toolkit/textEntry.cpp
	toolkit/tickableMenu.cpp
	toolkit/graph/connection.cpp
	toolkit/graph/inputSocket.cpp
	toolkit/graph/node.cpp
	toolkit/graph/outputSocket.cpp
	toolkit/graph/socket.cpp
	toolkit/graph/view.cpp
)

# NodeTree Dynamic Widgets header files
set(NODETREE_DYNAMIC_WIDGETS_HEADER_FILES
	nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/memoryObjectDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/listenerStreamConnectionWidget.hpp
	nodeTreeDynamicWidgets/talkerStreamConnectionWidget.hpp
	nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/streamFormatComboBox.hpp
	nodeTreeDynamicWidgets/asPathWidget.hpp
)

# NodeTree Dynamic Widgets source files
set(NODETREE_DYNAMIC_WIDGETS_SOURCE_FILES
	nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/memoryObjectDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/listenerStreamConnectionWidget.cpp
	nodeTreeDynamicWidgets/talkerStreamConnectionWidget.cpp
	nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/streamFormatComboBox.cpp
	nodeTreeDynamicWidgets/asPathWidget.cpp
)

# Avdecc helper header files
set(AVDECC_HELPER_HEADER_FILES
	avdecc/controllerManager.hpp
	avdecc/controllerModel.hpp
	avdecc/helper.hpp
	avdecc/hiveLogItems.hpp
	avdecc/loggerModel.hpp
	avdecc/stringValidator.hpp
)

# Avdecc helper source files
set(AVDECC_HELPER_SOURCE_FILES
	avdecc/controllerManager.cpp
	avdecc/controllerModel.cpp
	avdecc/helper.cpp
	avdecc/loggerModel.cpp
)

# Settings Dialog header files
set(SETTINGS_DIALOG_HEADER_FILES
	settingsDialog.hpp
)

# Settings Dialog source files
set(SETTINGS_DIALOG_SOURCE_FILES
	settingsDialog.cpp
)

# Settings Dialog resource files
set(SETTINGS_DIALOG_RESOURCE_FILES
	settingsDialog.ui
)

# About Dialog header files
set(ABOUT_DIALOG_HEADER_FILES
	aboutDialog.hpp
)

# About Dialog source files
set(ABOUT_DIALOG_SOURCE_FILES
	aboutDialog.cpp
)

# About Dialog resource files
set(ABOUT_DIALOG_RESOURCE_FILES
	aboutDialog.ui
)

# Connection Matriw header files
set(CONNECTION_MATRIX_HEADER_FILES
	connectionMatrix/cornerWidget.hpp
	connectionMatrix/model.hpp
	connectionMatrix/node.hpp
	connectionMatrix/view.hpp
	connectionMatrix/headerView.hpp
	connectionMatrix/itemDelegate.hpp
	connectionMatrix/paintHelper.hpp
)

# Connection Matrix source files
set(CONNECTION_MATRIX_SOURCE_FILES
	connectionMatrix/cornerWidget.cpp
	connectionMatrix/model.cpp
	connectionMatrix/node.cpp
	connectionMatrix/view.cpp
	connectionMatrix/headerView.cpp
	connectionMatrix/itemDelegate.cpp
	connectionMatrix/paintHelper.cpp
)

set(COUNTERS_HEADER_FILES
	counters/entityCountersTreeWidgetItem.hpp
	counters/avbInterfaceCountersTreeWidgetItem.hpp
	counters/clockDomainCountersTreeWidgetItem.hpp
	counters/streamInputCountersTreeWidgetItem.hpp
	counters/streamOutputCountersTreeWidgetItem.hpp
)

set(COUNTERS_SOURCE_FILES
	counters/entityCountersTreeWidgetItem.cpp
	counters/avbInterfaceCountersTreeWidgetItem.cpp
	counters/clockDomainCountersTreeWidgetItem.cpp
	counters/streamInputCountersTreeWidgetItem.cpp
	counters/streamOutputCountersTreeWidgetItem.cpp
)

# Logger View header files
set(LOGGER_VIEW_HEADER_FILES
	loggerView.hpp
)

# Logger View source files
set(LOGGER_VIEW_SOURCE_FILES
	loggerView.cpp
)

# Logger View resource files
set(LOGGER_VIEW_RESOURCE_FILES
	loggerView.ui
)

# Settings header files
set(SETTINGS_HEADER_FILES
	settingsManager/settingsManager.hpp
	settingsManager/settings.hpp
)

# Settings source files
set(SETTINGS_SOURCE_FILES
	settingsManager/settingsManager.cpp
)

# Updater header files
set(UPDATER_HEADER_FILES
	updater/updater.hpp
)

# Updater source files
set(UPDATER_SOURCE_FILES
	updater/updater.cpp
)

# Firmware Upload Dialog header files
set(FIRMWARE_UPLOAD_DIALOG_HEADER_FILES
	firmwareUploadDialog.hpp
)

# Firmware Upload Dialog source files
set(FIRMWARE_UPLOAD_DIALOG_SOURCE_FILES
	firmwareUploadDialog.cpp
)

# Firmware Upload Dialog resource files
set(FIRMWARE_UPLOAD_DIALOG_RESOURCE_FILES
	firmwareUploadDialog.ui
)

# Main windows header files
set(MAIN_WINDOW_HEADER_FILES
	mainWindow.hpp
)

# Main windows source files
set(MAIN_WINDOW_SOURCE_FILES
	mainWindow.cpp
)

# Main windows resource files
set(MAIN_WINDOW_RESOURCE_FILES
	mainWindow.ui
)

# Common Header files
set(COMMON_HEADER_FILES
	aecpCommandComboBox.hpp
	controlledEntityTreeWidget.hpp
	entityInspector.hpp
	entityLogoCache.hpp
	highlightForegroundItemDelegate.hpp
	imageItemDelegate.hpp
	mappingMatrix.hpp
	nodeTreeWidget.hpp
	nodeVisitor.hpp
	painterHelper.hpp
	settingsManager/settingsManager.hpp
)

# Common source files
set(COMMON_SOURCE_FILES
	aecpCommandComboBox.cpp
	entityLogoCache.cpp
	highlightForegroundItemDelegate.cpp
	imageItemDelegate.cpp
	main.cpp
	controlledEntityTreeWidget.cpp
	entityInspector.cpp
	nodeTreeWidget.cpp
	painterHelper.cpp
	nodeVisitor.cpp
)

# Group sources
source_group("Resource Files" FILES ${RESOURCE_FILES})
source_group("Resource Files\\About Dialog" FILES ${ABOUT_DIALOG_RESOURCE_FILES})
source_group("Resource Files\\Settings Dialog" FILES ${SETTINGS_DIALOG_RESOURCE_FILES})
source_group("Resource Files\\Logger View" FILES ${LOGGER_VIEW_RESOURCE_FILES})
source_group("Resource Files\\Settings" FILES ${SETTINGS_RESOURCE_FILES})
source_group("Resource Files\\Main Window" FILES ${MAIN_WINDOW_RESOURCE_FILES})
source_group("Resource Files\\Firmware Upload Dialog" FILES ${FIRMWARE_UPLOAD_DIALOG_RESOURCE_FILES})

source_group("Header Files\\Toolkit" FILES ${TOOLKIT_HEADER_FILES})
source_group("Header Files\\NodeTree Dynamic Widgets" FILES ${NODETREE_DYNAMIC_WIDGETS_HEADER_FILES})
source_group("Header Files\\Avdecc Helper" FILES ${AVDECC_HELPER_HEADER_FILES})
source_group("Header Files\\About Dialog" FILES ${ABOUT_DIALOG_HEADER_FILES})
source_group("Header Files\\Settings Dialog" FILES ${SETTINGS_DIALOG_HEADER_FILES})
source_group("Header Files\\Connection Matrix" FILES ${CONNECTION_MATRIX_HEADER_FILES})
source_group("Header Files\\Logger View" FILES ${LOGGER_VIEW_HEADER_FILES})
source_group("Header Files\\Settings" FILES ${SETTINGS_HEADER_FILES})
source_group("Header Files\\Updater" FILES ${UPDATER_HEADER_FILES})
source_group("Header Files\\Main Window" FILES ${MAIN_WINDOW_HEADER_FILES})
source_group("Header Files" FILES ${COMMON_HEADER_FILES})
source_group("Header Files\\Firmware Upload Dialog" FILES ${FIRMWARE_UPLOAD_DIALOG_HEADER_FILES})
source_group("Header Files\\Counters" FILES ${COUNTERS_HEADER_FILES})

source_group("Source Files\\Toolkit" FILES ${TOOLKIT_SOURCE_FILES})
source_group("Source Files\\NodeTree Dynamic Widgets" FILES ${NODETREE_DYNAMIC_WIDGETS_SOURCE_FILES})
source_group("Source Files\\Avdecc Helper" FILES ${AVDECC_HELPER_SOURCE_FILES})
source_group("Source Files\\About Dialog" FILES ${ABOUT_DIALOG_SOURCE_FILES})
source_group("Source Files\\Connection Matrix" FILES ${CONNECTION_MATRIX_SOURCE_FILES})
source_group("Source Files\\Settings Dialog" FILES ${SETTINGS_DIALOG_SOURCE_FILES})
source_group("Source Files\\Logger View" FILES ${LOGGER_VIEW_SOURCE_FILES})
source_group("Source Files\\Updater" FILES ${UPDATER_SOURCE_FILES})
source_group("Source Files\\Main Window" FILES ${MAIN_WINDOW_SOURCE_FILES})
source_group("Source Files" FILES ${COMMON_SOURCE_FILES})
source_group("Source Files\\Firmware Upload Dialog" FILES ${FIRMWARE_UPLOAD_DIALOG_SOURCE_FILES})
source_group("Source Files\\Counters" FILES ${COUNTERS_SOURCE_FILES})

# Executable creation
target_sources(${PROJECT_NAME} PUBLIC
	${RESOURCE_FILES}
	${ABOUT_DIALOG_RESOURCE_FILES}
	${SETTINGS_DIALOG_RESOURCE_FILES}
	${LOGGER_VIEW_RESOURCE_FILES}
	${MAIN_WINDOW_RESOURCE_FILES}
	${FIRMWARE_UPLOAD_DIALOG_RESOURCE_FILES}

	${TOOLKIT_HEADER_FILES}
	${NODETREE_DYNAMIC_WIDGETS_HEADER_FILES}
	${AVDECC_HELPER_HEADER_FILES}
	${ABOUT_DIALOG_HEADER_FILES}
	${CONNECTION_MATRIX_HEADER_FILES}
	${SETTINGS_DIALOG_HEADER_FILES}
	${LOGGER_VIEW_HEADER_FILES}
	${SETTINGS_HEADER_FILES}
	${UPDATER_HEADER_FILES}
	${MAIN_WINDOW_HEADER_FILES}
	${COMMON_HEADER_FILES}
	${FIRMWARE_UPLOAD_DIALOG_HEADER_FILES}
	${COUNTERS_HEADER_FILES}

	${TOOLKIT_SOURCE_FILES}
	${NODETREE_DYNAMIC_WIDGETS_SOURCE_FILES}
	${AVDECC_HELPER_SOURCE_FILES}
	${ABOUT_DIALOG_SOURCE_FILES}
	${CONNECTION_MATRIX_SOURCE_FILES}
	${SETTINGS_DIALOG_SOURCE_FILES}
	${LOGGER_VIEW_SOURCE_FILES}
	${SETTINGS_SOURCE_FILES}
	${UPDATER_SOURCE_FILES}
	${MAIN_WINDOW_SOURCE_FILES}
	${COMMON_SOURCE_FILES}
	${FIRMWARE_UPLOAD_DIALOG_SOURCE_FILES}
	${COUNTERS_SOURCE_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
	AUTORCC ON
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Widgets Qt5::Network la_avdecc_controller_cxx ${BUGREPORTER_LINK_LIBRARIES} libmarkdown)

# Include directories
target_include_directories(${PROJECT_NAME}
	PRIVATE
		$<BUILD_INTERFACE:${PROJECT_ROOT_DIR}/include>
		$<BUILD_INTERFACE:${PROJECT_ROOT_DIR}/src>
		$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)

include(TargetConfigureExecutable OPTIONAL RESULT_VARIABLE TargetConfigureExecutable_INCLUDED)
if(TargetConfigureExecutable_INCLUDED)
	target_configure_executable(${PROJECT_NAME})
endif()

include(TargetCopyLibraries OPTIONAL RESULT_VARIABLE TargetCopyLibraries_INCLUDED)
if(TargetCopyLibraries_INCLUDED)
	if(APPLE)
		# Do not use the INSTALL flag because the function currently do not strip symbols which messes with GateKeeper, let the deployqt do it for us (properly)
		set(targetCopyFlags "")
	else()
		set(targetCopyFlags "INSTALL")
	endif()
	target_copy_libraries(${PROJECT_NAME} ${targetCopyFlags})
endif()

include(TargetQt5Deploy OPTIONAL RESULT_VARIABLE TargetQt5Deploy_INCLUDED)
if(TargetQt5Deploy_INCLUDED)
	target_qt5_deploy(${PROJECT_NAME} INSTALL QML_DIR ${PROJECT_ROOT_DIR}/resources/qml)
endif()

# Sign target
if(ENABLE_HIVE_SIGNING)
	sign_target(${PROJECT_NAME})
endif()

# Install target
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin BUNDLE DESTINATION .)
