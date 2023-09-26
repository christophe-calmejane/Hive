# Hive Widget Models Library CMake File

# Declare project
cu_setup_project(${PROJECT_NAME}_widget_models ${HIVE_VERSION} "Hive Widget Models Library")

# Find dependencies
find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Widgets REQUIRED)

# Configure files based on CMakeLists.txt version number
configure_file(
	config.hpp.in
	${CMAKE_CURRENT_BINARY_DIR}/config.hpp
)

set(PCH_HEADER_NAME "pch.hpp")
set(PCH_SOURCE_NAME "pch.cpp")

# Define PCH files for windows
if(WIN32)
	set(PCH_FILES ${PCH_HEADER_NAME} ${PCH_SOURCE_NAME})
endif()

# Resource files
set(RESOURCE_FILES_GENERATED)

if(WIN32)
	configure_file(
		${HIVE_RESOURCES_FOLDER}/win32/WidgetModelsLibrary.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/WidgetModelsLibrary.rc
	)
	list(APPEND RESOURCE_FILES_GENERATED
		${CMAKE_CURRENT_BINARY_DIR}/WidgetModelsLibrary.rc
	)
endif()

set(HEADER_FILES_GENERATED
	${CMAKE_CURRENT_BINARY_DIR}/config.hpp
)

set(HEADER_FILES_PUBLIC
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/widgetModelsLibrary.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/painterHelper.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/entityLogoCache.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/qtUserRoles.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/errorIconItemDelegate.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/errorItemDelegate.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/imageItemDelegate.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/discoveredEntitiesTableItemDelegate.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/networkInterfacesListModel.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/discoveredEntitiesTableModel.hpp
	${CU_ROOT_DIR}/include/hive/widgetModelsLibrary/networkInterfacesListItemDelegate.hpp
)

set(HEADER_FILES_COMMON
)

set(SOURCE_FILES_COMMON
	widgetModelsLibrary.cpp
	painterHelper.cpp
	entityLogoCache.cpp
	errorIconItemDelegate.cpp
	errorItemDelegate.cpp
	imageItemDelegate.cpp
	discoveredEntitiesTableItemDelegate.cpp
	networkInterfacesListModel.cpp
	discoveredEntitiesTableModel.cpp
	networkInterfacesListItemDelegate.cpp
)

# Group source files
source_group("Header Files\\Public" FILES ${HEADER_FILES_PUBLIC})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Header Files" FILES ${HEADER_FILES_COMMON})
source_group("Header Files" FILES ${HEADER_FILES_GENERATED})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX "Source Files" FILES ${SOURCE_FILES_COMMON})
source_group(TREE ${HIVE_RESOURCES_FOLDER} PREFIX "Resource Files" FILES ${RESOURCE_FILES})
source_group("Resource Files" FILES ${RESOURCE_FILES_GENERATED})

# Define static library
add_library(${PROJECT_NAME}_static STATIC ${HEADER_FILES_PUBLIC} ${HEADER_FILES_COMMON} ${HEADER_FILES_GENERATED} ${SOURCE_FILES_COMMON} ${PCH_FILES})

# Setup common options
cu_setup_library_options(${PROJECT_NAME}_static)

# Enable precompiled headers
cu_set_precompiled_headers(${PROJECT_NAME}_static ${PCH_HEADER_NAME})

# Automatically generate MOC
set_target_properties(${PROJECT_NAME}_static PROPERTIES
	AUTOMOC ON
)

# Additional compile definitions
target_compile_definitions(${PROJECT_NAME}_static PRIVATE RESOURCES_ROOT_DIR="${HIVE_RESOURCES_FOLDER}")

# Link libraries
target_link_libraries(${PROJECT_NAME}_static PUBLIC Qt${QT_MAJOR_VERSION}::Widgets PRIVATE Hive_models_static QtMate_static)

if(BUILD_HIVE_MODELS_SHARED_LIBRARY)
	# Define shared library
	add_library(${PROJECT_NAME}_shared SHARED ${HEADER_FILES_PUBLIC} ${HEADER_FILES_COMMON} ${HEADER_FILES_GENERATED} ${SOURCE_FILES_COMMON} ${RESOURCE_FILES_GENERATED} ${PCH_FILES})

	# Setup common options
	cu_setup_library_options(${PROJECT_NAME}_shared)

	# Enable precompiled headers
	cu_set_precompiled_headers(${PROJECT_NAME}_shared ${PCH_HEADER_NAME})

	# Automatically generate MOC
	set_target_properties(${PROJECT_NAME}_shared PROPERTIES
		AUTOMOC ON
	)

	# Additional compile definitions
	target_compile_definitions(${PROJECT_NAME}_shared PRIVATE RESOURCES_ROOT_DIR="${HIVE_RESOURCES_FOLDER}")

	# Link libraries
	target_link_libraries(${PROJECT_NAME}_shared PUBLIC Qt${QT_MAJOR_VERSION}::Widgets PRIVATE Hive_models_static QtMate_static)

endif()

################
################
################
# Temporarily reduce warning level
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Clang and AppleClang
	target_compile_options(${PROJECT_NAME}_static PRIVATE -W -Wno-everything)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	target_compile_options(${PROJECT_NAME}_static PRIVATE -W -Wno-unused-variable -Wno-unused-but-set-variable -Wno-ignored-qualifiers -Wno-sign-compare)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
	# Don't use Wall on MSVC, it prints too many stupid warnings
	target_compile_options(${PROJECT_NAME}_static PRIVATE /W3)
else()
	message(FATAL_ERROR "Unsupported Compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

if(BUILD_HIVE_MODELS_SHARED_LIBRARY)
	if(CMAKE_CXX_COMPILER_ID MATCHES "Clang") # Clang and AppleClang
		target_compile_options(${PROJECT_NAME}_shared PRIVATE -W -Wno-everything)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		target_compile_options(${PROJECT_NAME}_shared PRIVATE -W -Wno-unused-variable -Wno-unused-but-set-variable -Wno-ignored-qualifiers -Wno-sign-compare)
	elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
		# Don't use Wall on MSVC, it prints too many stupid warnings
		target_compile_options(${PROJECT_NAME}_shared PRIVATE /W3)
	else()
		message(FATAL_ERROR "Unsupported Compiler: ${CMAKE_CXX_COMPILER_ID}")
	endif()
endif()
