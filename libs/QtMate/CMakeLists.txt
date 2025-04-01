# QtMate library

# Declare project
cu_setup_project(QtMate "1.0.0.1" "QtMate Library")

# Find dependencies
find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Widgets Xml Svg REQUIRED)

set(PCH_HEADER_NAME "pch.hpp")
set(PCH_SOURCE_NAME "pch.cpp")

# Define PCH files for windows
if(WIN32)
	set(PCH_FILES ${PCH_HEADER_NAME} ${PCH_SOURCE_NAME})
endif()

set(HEADER_FILES_PUBLIC
	${CU_ROOT_DIR}/include/QtMate/material/color.hpp
	${CU_ROOT_DIR}/include/QtMate/material/colorPalette.hpp
	${CU_ROOT_DIR}/include/QtMate/material/helper.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowConnection.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowDefs.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowInput.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowLink.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowNode.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowOutput.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowScene.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowSceneDelegate.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowSocket.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowStyle.hpp
	${CU_ROOT_DIR}/include/QtMate/flow/flowView.hpp
	${CU_ROOT_DIR}/include/QtMate/widgets/autoSizeLabel.hpp
	${CU_ROOT_DIR}/include/QtMate/widgets/comboBox.hpp
	${CU_ROOT_DIR}/include/QtMate/widgets/dynamicHeaderView.hpp
	${CU_ROOT_DIR}/include/QtMate/widgets/flatIconButton.hpp
	${CU_ROOT_DIR}/include/QtMate/widgets/headerViewSortSectionFilter.hpp
	${CU_ROOT_DIR}/include/QtMate/widgets/tableView.hpp
	${CU_ROOT_DIR}/include/QtMate/widgets/textEntry.hpp
	${CU_ROOT_DIR}/include/QtMate/widgets/tickableMenu.hpp
	${CU_ROOT_DIR}/include/QtMate/image/logoGenerator.hpp
	${CU_ROOT_DIR}/include/QtMate/image/svgUtils.hpp
)

set(HEADER_FILES_COMMON
)

set(SOURCE_FILES_COMMON
	material/color.cpp
	material/colorPalette.cpp
	material/helper.cpp
	flow/flowConnection.cpp
	flow/flowInput.cpp
	flow/flowLink.cpp
	flow/flowNode.cpp
	flow/flowOutput.cpp
	flow/flowScene.cpp
	flow/flowSceneDelegate.cpp
	flow/flowSocket.cpp
	flow/flowStyle.cpp
	flow/flowView.cpp
	widgets/autoSizeLabel.cpp
	widgets/comboBox.cpp
	widgets/dynamicHeaderView.cpp
	widgets/flatIconButton.cpp
	widgets/tableView.cpp
	widgets/textEntry.cpp
	widgets/tickableMenu.cpp
	image/logoGenerator.cpp
	image/svgUtils.cpp
)

# Group source files
source_group("Header Files\\Public" FILES ${HEADER_FILES_PUBLIC})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES_COMMON})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES_COMMON})

# Define static library to be used by main application and unit tests
add_library(${PROJECT_NAME}_static STATIC ${HEADER_FILES_PUBLIC} ${HEADER_FILES_COMMON} ${SOURCE_FILES_COMMON} ${PCH_FILES})

# Setup common options
cu_setup_library_options(${PROJECT_NAME}_static)

# Enable precompiled headers
cu_set_precompiled_headers(${PROJECT_NAME}_static ${PCH_HEADER_NAME})

# Automatically generate MOC
set_target_properties(${PROJECT_NAME}_static PROPERTIES
	AUTOMOC ON
)

# Link libraries
target_link_libraries(${PROJECT_NAME}_static PUBLIC Qt${QT_MAJOR_VERSION}::Widgets Qt${QT_MAJOR_VERSION}::Xml Qt${QT_MAJOR_VERSION}::Svg)

###### We unfortunately have to reduce compiler warnings due to Qt not willing to fix it's mess (https://bugreports.qt.io/browse/QTBUG-35620)
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# Don't use Wall on MSVC, it prints too many stupid warnings
	target_compile_options(${PROJECT_NAME}_static PRIVATE /W3)
endif()
