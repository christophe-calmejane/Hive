# Main project

# Declare project
project(${PROJECT_NAME} LANGUAGES C CXX VERSION ${HIVE_VERSION})

# Setup project version, because a release build only has 3 digits
set(PROJECT_VERSIONMAJ ${PROJECT_VERSION_MAJOR})
set(PROJECT_VERSIONMIN ${PROJECT_VERSION_MINOR})
set(PROJECT_VERSIONSUB ${PROJECT_VERSION_PATCH})
set(PROJECT_VERSIONREV ${PROJECT_VERSION_TWEAK})
if(NOT PROJECT_VERSIONREV)
	set(PROJECT_VERSIONREV 0)
endif()

# Declare executable target
add_executable(${PROJECT_NAME} MACOSX_BUNDLE WIN32 "")

# Configure file based on CMakeLists.txt
configure_file(
	internals/config.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/internals/config.hpp
)

# Resource files
set(RESOURCE_FILES
	${PROJECT_ROOT_DIR}/resources/main.qrc
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
	toolkit/matrixTreeView.hpp
	toolkit/dynamicHeaderView.hpp
	toolkit/tableView.hpp
	toolkit/textEntry.hpp
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
	toolkit/matrixTreeView.cpp
	toolkit/dynamicHeaderView.cpp
	toolkit/tableView.cpp
	toolkit/textEntry.cpp
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
	nodeTreeDynamicWidgets/streamConnectionWidget.hpp
	nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.hpp
	nodeTreeDynamicWidgets/streamFormatComboBox.hpp
)

# NodeTree Dynamic Widgets source files
set(NODETREE_DYNAMIC_WIDGETS_SOURCE_FILES
	nodeTreeDynamicWidgets/audioUnitDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/avbInterfaceDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/streamConnectionWidget.cpp
	nodeTreeDynamicWidgets/streamDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/streamPortDynamicTreeWidgetItem.cpp
	nodeTreeDynamicWidgets/streamFormatComboBox.cpp
)

# Avdecc helper header files
set(AVDECC_HELPER_HEADER_FILES
	avdecc/controllerManager.hpp
	avdecc/controllerModel.hpp
	avdecc/helper.hpp
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
	acquireStateItemDelegate.hpp
	connectionMatrix.hpp
	controlledEntityTreeWidget.hpp
	entityInspector.hpp
	mappingMatrix.hpp
	nodeTreeWidget.hpp
	nodeVisitor.hpp
)

# Common source files
set(COMMON_SOURCE_FILES
	acquireStateItemDelegate.cpp
	main.cpp
	connectionMatrix.cpp
	controlledEntityTreeWidget.cpp
	entityInspector.cpp
	nodeTreeWidget.cpp
	nodeVisitor.cpp
)

# Group sources
source_group("Resource Files" FILES ${RESOURCE_FILES})
source_group("Resource Files\\About Dialog" FILES ${ABOUT_DIALOG_RESOURCE_FILES})
source_group("Resource Files\\Logger View" FILES ${LOGGER_VIEW_RESOURCE_FILES})
source_group("Resource Files\\Main Window" FILES ${MAIN_WINDOW_RESOURCE_FILES})

source_group("Header Files\\Toolkit" FILES ${TOOLKIT_HEADER_FILES})
source_group("Header Files\\NodeTree Dynamic Widgets" FILES ${NODETREE_DYNAMIC_WIDGETS_HEADER_FILES})
source_group("Header Files\\Avdecc Helper" FILES ${AVDECC_HELPER_HEADER_FILES})
source_group("Header Files\\About Dialog" FILES ${ABOUT_DIALOG_HEADER_FILES})
source_group("Header Files\\Logger View" FILES ${LOGGER_VIEW_HEADER_FILES})
source_group("Header Files\\Main Window" FILES ${MAIN_WINDOW_HEADER_FILES})
source_group("Header Files" FILES ${COMMON_HEADER_FILES})

source_group("Source Files\\Toolkit" FILES ${TOOLKIT_SOURCE_FILES})
source_group("Source Files\\NodeTree Dynamic Widgets" FILES ${NODETREE_DYNAMIC_WIDGETS_SOURCE_FILES})
source_group("Source Files\\Avdecc Helper" FILES ${AVDECC_HELPER_SOURCE_FILES})
source_group("Source Files\\About Dialog" FILES ${ABOUT_DIALOG_SOURCE_FILES})
source_group("Source Files\\Logger View" FILES ${LOGGER_VIEW_SOURCE_FILES})
source_group("Source Files\\Main Window" FILES ${MAIN_WINDOW_SOURCE_FILES})
source_group("Source Files" FILES ${COMMON_SOURCE_FILES})

# Executable creation
target_sources(${PROJECT_NAME} PUBLIC
	${RESOURCE_FILES}
	${ABOUT_DIALOG_RESOURCE_FILES}
	${LOGGER_VIEW_RESOURCE_FILES}
	${MAIN_WINDOW_RESOURCE_FILES}

	${TOOLKIT_HEADER_FILES}
	${NODETREE_DYNAMIC_WIDGETS_HEADER_FILES}
	${AVDECC_HELPER_HEADER_FILES}
	${ABOUT_DIALOG_HEADER_FILES}
	${LOGGER_VIEW_HEADER_FILES}
	${MAIN_WINDOW_HEADER_FILES}
	${COMMON_HEADER_FILES}

	${TOOLKIT_SOURCE_FILES}
	${NODETREE_DYNAMIC_WIDGETS_SOURCE_FILES}
	${AVDECC_HELPER_SOURCE_FILES}
	${ABOUT_DIALOG_SOURCE_FILES}
	${LOGGER_VIEW_SOURCE_FILES}
	${MAIN_WINDOW_SOURCE_FILES}
	${COMMON_SOURCE_FILES}
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	CXX_STANDARD 14
	CXX_STANDARD_REQUIRED ON
	DEBUG_POSTFIX "-d"
)

set_target_properties(${PROJECT_NAME} PROPERTIES
	AUTOMOC ON
	AUTOUIC ON
	AUTORCC ON
)

set_debug_define(${PROJECT_NAME})
	
# Link libraries
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Widgets Qt5::WebEngineWidgets la_avdecc_controller_cxx ${BUGREPORTER_LINK_LIBRARIES})

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
	target_copy_libraries(${PROJECT_NAME} INSTALL)
endif()

include(TargetQt5Deploy OPTIONAL RESULT_VARIABLE TargetQt5Deploy_INCLUDED)
if(TargetQt5Deploy_INCLUDED)
	target_qt5_deploy(${PROJECT_NAME} INSTALL QML_DIR ${PROJECT_ROOT_DIR}/resources/qml)
endif()

install(TARGETS ${PROJECT_NAME}	RUNTIME DESTINATION bin	BUNDLE DESTINATION .)
