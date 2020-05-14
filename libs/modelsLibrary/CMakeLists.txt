# Hive Models Library CMake File

# Declare project
setup_project(${PROJECT_NAME}_models ${HIVE_VERSION} "Hive Models Library")

# Find dependencies
find_package(Qt5 COMPONENTS Widgets REQUIRED)

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
		${HIVE_RESOURCES_FOLDER}/win32/ModelsLibrary.rc.in
		${CMAKE_CURRENT_BINARY_DIR}/ModelsLibrary.rc
	)
	list(APPEND RESOURCE_FILES_GENERATED
		${CMAKE_CURRENT_BINARY_DIR}/ModelsLibrary.rc
	)
endif()

set(HEADER_FILES_GENERATED
	${CMAKE_CURRENT_BINARY_DIR}/config.hpp
)

set(HEADER_FILES_PUBLIC
	${PROJECT_ROOT_DIR}/include/hive/modelsLibrary/modelsLibrary.hpp
	${PROJECT_ROOT_DIR}/include/hive/modelsLibrary/helper.hpp
	${PROJECT_ROOT_DIR}/include/hive/modelsLibrary/controllerManager.hpp
	${PROJECT_ROOT_DIR}/include/hive/modelsLibrary/networkInterfacesModel.hpp
	${PROJECT_ROOT_DIR}/include/hive/modelsLibrary/discoveredEntitiesModel.hpp
)

set(HEADER_FILES_COMMON
)

set(SOURCE_FILES_COMMON
	modelsLibrary.cpp
	helper.cpp
	controllerManager.cpp
	networkInterfacesModel.cpp
	discoveredEntitiesModel.cpp
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

# Enable precompiled headers
set_precompiled_headers(${PROJECT_NAME}_static ${PCH_HEADER_NAME} ${PCH_SOURCE_NAME})

# Setup common options
setup_library_options(${PROJECT_NAME}_static "${PROJECT_NAME}_static")

set_target_properties(${PROJECT_NAME}_static PROPERTIES
	AUTOMOC ON
)

# Additional compile definitions
target_compile_definitions(${PROJECT_NAME}_static PRIVATE RESOURCES_ROOT_DIR="${HIVE_RESOURCES_FOLDER}")

# Link libraries
target_link_libraries(${PROJECT_NAME}_static PUBLIC Qt5::Core la_avdecc_controller_cxx)

if(BUILD_HIVE_MODELS_SHARED_LIBRARY)
	# Define shared library
	add_library(${PROJECT_NAME}_shared SHARED ${HEADER_FILES_PUBLIC} ${HEADER_FILES_COMMON} ${HEADER_FILES_GENERATED} ${SOURCE_FILES_COMMON} ${RESOURCE_FILES_GENERATED} ${PCH_FILES})

	# Enable precompiled headers
	set_precompiled_headers(${PROJECT_NAME}_shared ${PCH_HEADER_NAME} ${PCH_SOURCE_NAME})

	# Setup common options
	setup_library_options(${PROJECT_NAME}_shared "${PROJECT_NAME}_shared")

	set_target_properties(${PROJECT_NAME}_shared PROPERTIES
		AUTOMOC ON
	)

	# Additional compile definitions
	target_compile_definitions(${PROJECT_NAME}_shared PRIVATE RESOURCES_ROOT_DIR="${HIVE_RESOURCES_FOLDER}")

	# Link libraries
	target_link_libraries(${PROJECT_NAME}_shared PUBLIC Qt5::Core la_avdecc_controller_cxx)

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
