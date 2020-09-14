# Root CMake File for Hive

# Minimum CMake version
cmake_minimum_required(VERSION 3.15)

############ Versioning

# Using Semantic Versioning (http://semver.org/spec/v2.0.0.html)
# This variable defines the project version number.
# It should have 3 digits (x.y.z) on main branch, and 4 digits (x.y.z.w) on dev and task branches. The last digit being used as beta label.
# As soon as a new version begins, update the version number by following SemVer rules.
# Example: 1.5.0 is the upcoming release version (main branch) of current 1.5.0.3 devel version (dev/task) which will be labelled 1.5.0-beta3
set(HIVE_VERSION 1.2.3)

############ Override from command line "CMake -D<OPTION>=TRUE/FALSE/0/1/ON/OFF"

# Build options
option(BUILD_HIVE_TESTS "Build Hive tests." TRUE)
option(BUILD_HIVE_APPLICATION "Build Hive main application." TRUE)
# Install options
option(ENABLE_HIVE_CPACK "Enable Hive installer generation target." TRUE)
# Signing options
option(ENABLE_HIVE_SIGNING "Enable binaries signing." FALSE)
# Enable features
option(ENABLE_HIVE_FEATURE_SPARKLE "Use Sparkle for Hive auto-update" TRUE)

############ Override from command line "CMake -D<PARAM>=<VALUE>"

# Configuration options
if(NOT HIVE_APPCAST_RELEASES_URL)
	set(HIVE_APPCAST_RELEASES_URL "https://localhost/hive/appcast-release.xml" CACHE STRING "Appcast URL for Hive Releases" FORCE)
endif()
message(STATUS "Appcast URL for Hive Releases defined to: ${HIVE_APPCAST_RELEASES_URL}")
if(NOT HIVE_APPCAST_BETAS_URL)
	set(HIVE_APPCAST_BETAS_URL "https://localhost/hive/appcast-beta.xml" CACHE STRING "Appcast URL for Hive Betas" FORCE)
endif()
message(STATUS "Appcast URL for Hive Betas defined to: ${HIVE_APPCAST_BETAS_URL}")
if(APPLE AND NOT HIVE_DSA_PUB_KEY)
	message(FATAL_ERROR "DSA Public Key not set in HIVE_DSA_PUB_KEY CMake Variable")
endif()

############ CMake Configuration

project(Hive)

# Using c++17 standard for all targets
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Default Component
set(CMAKE_INSTALL_DEFAULT_COMPONENT_NAME "Hive")

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

# Include our internal macros
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake) # Required to find_package 3rd party modules that don't provide their CMake Config files
include(cmake/TargetSetupDeploy.cmake)

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
set(BUILD_AVDECC_BINDINGS_C OFF CACHE BOOL "Build avdecc C bindings library." FORCE)
set(BUILD_AVDECC_CONTROLLER ON CACHE BOOL "Build avdecc controller library." FORCE)
set(BUILD_AVDECC_INTERFACE_PCAP ON CACHE BOOL "Build the pcap protocol interface." FORCE)
set(BUILD_AVDECC_INTERFACE_PCAP_DYNAMIC_LINKING ON CACHE BOOL "Pcap protocol interface uses dynamic shared library linking (instead of static linking)." FORCE)
set(BUILD_AVDECC_INTERFACE_MAC ON CACHE BOOL "Build the macOS native protocol interface (macOS only)." FORCE)
set(BUILD_AVDECC_INTERFACE_PROXY OFF CACHE BOOL "Build the proxy protocol interface." FORCE)
set(BUILD_AVDECC_INTERFACE_VIRTUAL ON CACHE BOOL "Build the virtual protocol interface (for unit tests)." FORCE)
# Install options
set(INSTALL_AVDECC_EXAMPLES OFF CACHE BOOL "Install examples." FORCE)
set(INSTALL_AVDECC_TESTS OFF CACHE BOOL "Install unit tests." FORCE)
set(INSTALL_AVDECC_LIB_SHARED_CXX OFF CACHE BOOL "Install C++ shared library." FORCE) # Don't install the libraries, we copy them manually (we want them inside the Bundle on macOS)
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
set(ALLOW_GET_AUDIO_MAP_UNSOL ON CACHE BOOL "Allow receiving GetAudioMap unsolicited notifications." FORCE)
set(ALLOW_RECV_BIG_AECP_PAYLOADS ON CACHE BOOL "Allow receiving AECP payloads bigger than the allowed 524 bytes." FORCE)
set(ALLOW_SEND_BIG_AECP_PAYLOADS OFF CACHE BOOL "Allow sending AECP payloads bigger than the allowed 524 bytes." FORCE)

# Include avdecc project
add_subdirectory(${AVDECC_BASE_FOLDER})

# Include avdecc internal macros
include(${AVDECC_BASE_FOLDER}/cmake/internals.cmake)

# Override discount options
set(DISCOUNT_MAKE_INSTALL OFF CACHE BOOL "Disable install rules" FORCE)
set(DISCOUNT_INSTALL_SAMPLES OFF CACHE BOOL "Disable install samples" FORCE)
set(DISCOUNT_ONLY_LIBRARY ON CACHE BOOL "Disable all samples" FORCE)

# Include discount project
add_subdirectory(3rdparty/discount/cmake/)

# Set folder for markdown library targets
target_compile_options(libmarkdown PRIVATE -DDESTRUCTOR=)
set_target_properties(libmarkdown PROPERTIES FOLDER "Libraries/Markdown")
set_target_properties(mktags PROPERTIES FOLDER "Libraries/Markdown")

# Set Folder for FMT library
if(ENABLE_AVDECC_USE_FMTLIB)
	set_target_properties(fmt PROPERTIES FOLDER "Libraries")
endif()

############ Define some variables

string(TIMESTAMP CURRENT_YEAR "%Y")
set(PROJECT_COPYRIGHT_YEAR "2017-${CURRENT_YEAR}")
set(PROJECT_FULL_NAME "${PROJECT_NAME} - Pro Audio Avdecc Controller")
set(PROJECT_COMPANYNAME "KikiSoft")
set(PROJECT_DOMAIN_COUNTRY "fr")
set(PROJECT_ORIGINAL_AUTHORS "Emilien Vallot and Christophe Calmejane")
set(PROJECT_AUTHORS "Emilien Vallot, Christophe Calmejane and other contributors")
set(PROJECT_CONTACT "christophe.calmejane@l-acoustics.com")
set(PROJECT_REVERSE_DOMAINNAME "${PROJECT_DOMAIN_COUNTRY}.${PROJECT_COMPANYNAME}")
set(PROJECT_DOMAINNAME "${PROJECT_COMPANYNAME}.${PROJECT_DOMAIN_COUNTRY}")
set(PROJECT_URL "https://github.com/christophe-calmejane/Hive")
set(PROJECT_LEGALCOPYRIGHT "(c) ${PROJECT_AUTHORS}")
set(PROJECT_READABLE_COPYRIGHT "Copyright ${PROJECT_COPYRIGHT_YEAR}, ${PROJECT_AUTHORS}")
set(PROJECT_ROOT_DIR "${PROJECT_SOURCE_DIR}")
set(PROJECT_ARCH "32-bit")
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(PROJECT_ARCH "64-bit")
endif()

# Split HIVE_VERSION string
string(REGEX MATCHALL "([0-9]+)" HIVE_VERSION_SPLIT "${HIVE_VERSION}")
list(LENGTH HIVE_VERSION_SPLIT HIVE_VERSION_SPLIT_LENGTH)
if(${HIVE_VERSION_SPLIT_LENGTH} LESS 3)
	message(FATAL_ERROR "Cannot parse version string")
endif()
list(GET HIVE_VERSION_SPLIT 0 HIVE_VERSION_MAJOR)
list(GET HIVE_VERSION_SPLIT 1 HIVE_VERSION_MINOR)
list(GET HIVE_VERSION_SPLIT 2 HIVE_VERSION_PATCH)
if(${HIVE_VERSION_SPLIT_LENGTH} EQUAL 4)
	list(GET HIVE_VERSION_SPLIT 3 HIVE_VERSION_BETA)
else()
	set(HIVE_VERSION_BETA "0")
endif()

# Compute Marketing Version String (Visible)
if(NOT DEFINED MARKETING_VERSION_DIGITS)
	set(MARKETING_VERSION_DIGITS 2)
endif()
set(HIVE_MARKETING_VERSION "")
if(${MARKETING_VERSION_DIGITS} GREATER 0)
	set(HIVE_MARKETING_VERSION "${HIVE_VERSION_MAJOR}")
	if(${MARKETING_VERSION_DIGITS} GREATER 1)
		math(EXPR LOOP_COUNT "${MARKETING_VERSION_DIGITS} - 1")
		foreach(index RANGE 1 ${LOOP_COUNT})
			list(GET HIVE_VERSION_SPLIT ${index} LOOP_VERSION)
			string(APPEND HIVE_MARKETING_VERSION ".${LOOP_VERSION}")
		endforeach()
	endif()
endif()
if(${MARKETING_VERSION_POSTFIX})
	string(APPEND HIVE_MARKETING_VERSION "${MARKETING_VERSION_POSTFIX}")
endif()

# Compute Full Friendly Version String (Visible)
if(${HIVE_VERSION_BETA} STREQUAL "0")
	set(HIVE_FRIENDLY_VERSION "${HIVE_VERSION_MAJOR}.${HIVE_VERSION_MINOR}.${HIVE_VERSION_PATCH}")
else()
	set(HIVE_FRIENDLY_VERSION "${HIVE_VERSION_MAJOR}.${HIVE_VERSION_MINOR}.${HIVE_VERSION_PATCH}-beta${HIVE_VERSION_BETA}")
endif()

############ Add projects

# Add main project
add_subdirectory(src)

# Add tools
add_subdirectory(tools)

# Add tests
if(BUILD_HIVE_TESTS)
	message(STATUS "Building Hive tests")
	# Setup gtest options
	set(gtest_force_shared_crt ON CACHE BOOL "For shared crt for gtest" FORCE)
	set(BUILD_GMOCK OFF CACHE BOOL "Don't build the googlemock subproject" FORCE)
	set(INSTALL_GTEST OFF CACHE BOOL "Disable installation of googletest. (Projects embedding googletest may want to turn this OFF.)" FORCE)
	# Include gtest framework
	add_subdirectory(${AVDECC_BASE_FOLDER}/externals/3rdparty/gtest)
	# Include our unit tests
	add_subdirectory(tests)
endif()

# Force Project as visual default target
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${PROJECT_NAME})

# Add CPack
if(ENABLE_HIVE_CPACK)
	include(cmake/CPackConfig.cmake)
endif()
