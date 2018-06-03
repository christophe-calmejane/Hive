# Tests samples

# Declare project
project(TestConnections LANGUAGES C CXX VERSION "1.0.0.0")

# Declare executable target
add_executable(${PROJECT_NAME} MACOSX_BUNDLE WIN32 "")

# Toolkit header files
set(TOOLKIT_HEADER_FILES
	${PROJECT_ROOT_DIR}/src/toolkit/graph/connection.hpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/inputSocket.hpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/node.hpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/outputSocket.hpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/socket.hpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/view.hpp
)

# Toolkit source files
set(TOOLKIT_SOURCE_FILES
	${PROJECT_ROOT_DIR}/src/toolkit/graph/connection.cpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/inputSocket.cpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/node.cpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/outputSocket.cpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/socket.cpp
	${PROJECT_ROOT_DIR}/src/toolkit/graph/view.cpp
)

# Common Header files
set(COMMON_HEADER_FILES
	${PROJECT_ROOT_DIR}/src/mappingMatrix.hpp
)

# Common source files
set(COMMON_SOURCE_FILES
	testConnections.cpp
)

# Group sources
source_group("Header Files\\Toolkit" FILES ${TOOLKIT_HEADER_FILES})
source_group("Header Files" FILES ${COMMON_HEADER_FILES})

source_group("Source Files\\Toolkit" FILES ${TOOLKIT_SOURCE_FILES})
source_group("Source Files" FILES ${COMMON_SOURCE_FILES})


# Executable creation
target_sources(${PROJECT_NAME} PUBLIC
	${TOOLKIT_HEADER_FILES}
	${COMMON_HEADER_FILES}

	${TOOLKIT_SOURCE_FILES}
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
target_link_libraries(${PROJECT_NAME} PUBLIC Qt5::Widgets)

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
