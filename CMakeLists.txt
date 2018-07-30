# Hive - Pro Audio Avdecc Controller

# Minimum CMake version
cmake_minimum_required(VERSION 3.10)

############ Versioning

# Using Semantic Versioning (http://semver.org/spec/v2.0.0.html)
# Project version number x.y.z on main branch, x.y.z.w on dev and task branches
# Example: 1.5.0 is the upcoming release version (main branch) of current 1.5.0.3 devel version (dev/task)
set(HIVE_VERSION 1.0.4)

############ Override from command line "CMake -D<OPTION>=TRUE/FALSE/0/1/ON/OFF"

# Build options
# Install options
option(ENABLE_HIVE_CPACK "Enable Hive installer generation target." TRUE)
# Signing options
option(ENABLE_HIVE_SIGNING "Enable binaries signing." FALSE)

############ CMake configuration

project(Hive)

# Using c++17 standard for all targets
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Set minimum OSX version
set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9 CACHE INTERNAL "Force the target to be at least a Mac OS X 10.9" FORCE)

# Enable cmake folders
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Configure installation path: we override the default installation path.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "./Install" CACHE PATH "default install path" FORCE)
endif()

# Setup "Release" build type, if not specified
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	message(STATUS "Setting build type to 'Release' as none was specified.")
	set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Specifies the build type." FORCE)
endif()

# Add our cmake modules & packages
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake)

# Find Qt dependencies
find_package(Qt5 COMPONENTS Widgets REQUIRED)

# Avdecc project location
if(NOT AVDECC_BASE_FOLDER)
	set(AVDECC_BASE_FOLDER "3rdparty/avdecc")
endif()

# Override avdecc options
set(BUILD_AVDECC_EXAMPLES OFF CACHE BOOL "Build examples." FORCE)
set(BUILD_AVDECC_TESTS OFF CACHE BOOL "Build unit tests." FORCE)
set(BUILD_AVDECC_LIB_SHARED_CXX ON CACHE BOOL "Build C++ shared library." FORCE)
set(BUILD_AVDECC_LIB_STATIC_RT_SHARED OFF CACHE BOOL "Build static library (runtime shared)." FORCE)
set(BUILD_AVDECC_DOC OFF CACHE BOOL "Build documentation." FORCE)
set(BUILD_AVDECC_WRAPPER_LUA OFF CACHE BOOL "Build avdecc lua wrapper library." FORCE)
set(BUILD_AVDECC_CONTROLLER ON CACHE BOOL "Build avdecc controller library." FORCE)
set(BUILD_AVDECC_INTERFACE_PCAP ON CACHE BOOL "Build the pcap protocol interface." FORCE)
set(BUILD_AVDECC_INTERFACE_PCAP_DYNAMIC_LINKING ON CACHE BOOL "Pcap protocol interface uses dynamic shared library linking (instead of static linking)." FORCE)
set(BUILD_AVDECC_INTERFACE_MAC ON CACHE BOOL "Build the macOS native protocol interface (macOS only)." FORCE)
set(BUILD_AVDECC_INTERFACE_PROXY OFF CACHE BOOL "Build the proxy protocol interface." FORCE)
set(BUILD_AVDECC_INTERFACE_VIRTUAL ON CACHE BOOL "Build the virtual protocol interface (for unit tests)." FORCE)
# Install options
set(INSTALL_AVDECC_EXAMPLES OFF CACHE BOOL "Install examples." FORCE)
set(INSTALL_AVDECC_TESTS OFF CACHE BOOL "Install unit tests." FORCE)
set(INSTALL_AVDECC_LIB_SHARED_CXX OFF CACHE BOOL "Install C++ shared library." FORCE)
set(INSTALL_AVDECC_LIB_STATIC OFF CACHE BOOL "Install static library." FORCE)
set(INSTALL_AVDECC_HEADERS OFF CACHE BOOL "Install headers." FORCE)
set(INSTALL_AVDECC_DOC OFF CACHE BOOL "Install documentation." FORCE)
# Signing options
set(ENABLE_AVDECC_SIGNING ${ENABLE_HIVE_SIGNING} CACHE BOOL "Enable binaries signing." FORCE)
# Enable features
set(ENABLE_AVDECC_FEATURE_REDUNDANCY ON CACHE BOOL "Enable 'Network Redundancy' feature as defined by AVnu Alliance." FORCE)
# Compatibility options
set(ENABLE_AVDECC_USE_FMTLIB ON CACHE BOOL "Use fmtlib" FORCE)
set(ENABLE_AVDECC_CUSTOM_ANY ON CACHE BOOL "Use custom std::any instead of c++17 one (for compilers not supporting std::any yet)" FORCE)
set(ENABLE_AVDECC_STRICT_2018_REDUNDANCY ON CACHE BOOL "Be strict about 'Network Redundancy' feature, using AVnu 2018 specifications." FORCE)
set(IGNORE_INVALID_CONTROL_DATA_LENGTH ON CACHE BOOL "Allow messages with an invalid advertised 'Control Data Length' field." FORCE)
set(IGNORE_INVALID_NON_SUCCESS_AEM_RESPONSES ON CACHE BOOL "Allow invalid AEM non-success responses messages." FORCE)
set(IGNORE_NEITHER_STATIC_NOR_DYNAMIC_MAPPINGS ON CACHE BOOL "Allow Entities not supporting Dynamic Mapping and not adversiting AUDIO_MAP." FORCE)
set(ALLOW_BIG_AEM_PAYLOADS ON CACHE BOOL "Allow AEM payloads bigger than the allowed 524 bytes." FORCE)

# Include avdecc project
add_subdirectory(${AVDECC_BASE_FOLDER})

# Include avdecc internal macros
include(${AVDECC_BASE_FOLDER}/cmake/internals.cmake)

############ Define some variables

string(TIMESTAMP CURRENT_YEAR "%Y")
set(PROJECT_COPYRIGHT_YEAR "2017-${CURRENT_YEAR}")
set(PROJECT_FULL_NAME "${PROJECT_NAME} - Pro Audio Avdecc Controller")
set(PROJECT_COMPANYNAME "KikiSoft")
set(PROJECT_AUTHORS "Emilien Vallot, Christophe Calmejane and other contributors")
set(PROJECT_DOMAINNAME "fr.${PROJECT_COMPANYNAME}")
set(PROJECT_URL "https://github.com/christophe-calmejane/Hive")
set(PROJECT_LEGALCOPYRIGHT "(c) ${PROJECT_AUTHORS}")
set(PROJECT_READABLE_COPYRIGHT "Copyright ${PROJECT_COPYRIGHT_YEAR}, ${PROJECT_AUTHORS}")
set(PROJECT_ROOT_DIR "${PROJECT_SOURCE_DIR}")
set(PROJECT_ARCH "32-bit")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(PROJECT_ARCH "64-bit")
endif()

############ Add projects

# Add main project
add_subdirectory(src)

# Add tests
add_subdirectory(tests/src)

# Force Project as visual default target
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${PROJECT_NAME})

# Add CPack
if(ENABLE_HIVE_CPACK)
	include(cmake/CPackConfig.cmake)
endif()
