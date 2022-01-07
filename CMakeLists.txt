# Root CMake File for Hive

# Minimum CMake version
cmake_minimum_required(VERSION 3.18.4)

############ Versioning

# Using Semantic Versioning (http://semver.org/spec/v2.0.0.html)
# This variable defines the project version number.
# It should have 3 digits (x.y.z) on main branch, and 4 digits (x.y.z.w) on dev and task branches. The last digit being used as beta label.
# As soon as a new version begins, update the version number by following SemVer rules.
# Example: 1.5.0 is the upcoming release version (main branch) of current 1.5.0.3 devel version (dev/task) which will be labelled 1.5.0-beta3
set(HIVE_VERSION 1.2.8.3)

############ Override from command line "CMake -D<OPTION>=TRUE/FALSE/0/1/ON/OFF"

# Build options
option(BUILD_HIVE_TESTS "Build Hive tests." FALSE)
option(BUILD_HIVE_MODELS_SHARED_LIBRARY "Build Hive Models Shared Library (for external usage)." FALSE)
option(BUILD_HIVE_APPLICATION "Build Hive main application." TRUE)
# Install options
option(ENABLE_HIVE_CPACK "Enable Hive installer generation target." FALSE)
# Signing options
option(ENABLE_CODE_SIGNING "Enable binaries signing." FALSE)
# Enable features
option(ENABLE_HIVE_FEATURE_SPARKLE "Use Sparkle for Hive auto-update" FALSE)

############ Override from command line "CMake -D<PARAM>=<VALUE>"

# Configuration options
if(ENABLE_HIVE_FEATURE_SPARKLE)
	if(NOT APPCAST_RELEASES_URL)
		set(APPCAST_RELEASES_URL "https://localhost/hive/appcast-release.xml" CACHE STRING "Appcast URL for Hive Releases" FORCE)
	endif()
	message(STATUS "Appcast URL for Hive Releases defined to: ${APPCAST_RELEASES_URL}")
	if(NOT APPCAST_BETAS_URL)
		set(APPCAST_BETAS_URL "https://localhost/hive/appcast-beta.xml" CACHE STRING "Appcast URL for Hive Betas" FORCE)
	endif()
	message(STATUS "Appcast URL for Hive Betas defined to: ${APPCAST_BETAS_URL}")
	if(NOT APPCAST_RELEASES_FALLBACK_URL)
		set(APPCAST_RELEASES_FALLBACK_URL "${APPCAST_RELEASES_URL}" CACHE STRING "Appcast Fallback URL for Hive Releases" FORCE)
	endif()
	message(STATUS "Appcast fallback URL for Hive Releases defined to: ${APPCAST_RELEASES_FALLBACK_URL}")
	if(NOT APPCAST_BETAS_FALLBACK_URL)
		set(APPCAST_BETAS_FALLBACK_URL "${APPCAST_BETAS_URL}" CACHE STRING "Appcast Fallback URL for Hive Betas" FORCE)
	endif()
	message(STATUS "Appcast fallback URL for Hive Betas defined to: ${APPCAST_BETAS_FALLBACK_URL}")
	if(APPLE AND NOT HIVE_DSA_PUB_KEY)
	message(FATAL_ERROR "DSA Public Key not set in HIVE_DSA_PUB_KEY CMake Variable")
	endif()
endif()
if(NOT QT_MAJOR_VERSION)
	set(QT_MAJOR_VERSION 5)
endif()

############ CMake Configuration

project(Hive)

# Using c++17 standard for all targets
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# Define some product/company specific values
set(CU_PROJECT_FULL_NAME "${PROJECT_NAME} - Pro Audio ATDECC Controller")
set(CU_COMPANY_NAME "KikiSoft")
set(CU_COMPANY_URL "https://github.com/christophe-calmejane/Hive")
set(CU_COMPANY_DOMAIN "com")
set(CU_PROJECT_PRODUCTDESCRIPTION "${CU_PROJECT_FULL_NAME}")
set(CU_PROJECT_BUNDLEIDENTIFIER "${CU_COMPANY_DOMAIN}.${CU_COMPANY_NAME}.${PROJECT_NAME}")
set(CU_PROJECT_CONTACT "christophe.calmejane@l-acoustics.com")
set(CU_PROJECT_STARTING_YEAR "2017")
set(CU_COPYRIGHT_HOLDER "Emilien Vallot, Christophe Calmejane and other contributors")

# Avdecc project location
if(NOT AVDECC_BASE_FOLDER)
	set(AVDECC_BASE_FOLDER "${CMAKE_CURRENT_LIST_DIR}/3rdparty/avdecc")
endif()

# Define base folder for cmakeUtils scripts
set(CU_BASE_DIR "${AVDECC_BASE_FOLDER}/cmake/cmakeUtils")

# Include project macros
include(${CU_BASE_DIR}/ProjectMacros.cmake)

# Include common properties overrides
include(${CU_BASE_DIR}/ProjectProperties.cmake)

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

# Add SparkleHelper targets
if(ENABLE_HIVE_FEATURE_SPARKLE)
	add_subdirectory(3rdparty/sparkleHelper)
endif()

# Add our cmake modules path
list(APPEND CMAKE_PREFIX_PATH ${CMAKE_CURRENT_LIST_DIR}/cmake) # Required to find_package 3rd party modules that don't provide their CMake Config files

# Find Qt dependencies
find_package(Qt${QT_MAJOR_VERSION} COMPONENTS Widgets;Network REQUIRED)

# Setup variables related to the project version
cu_setup_project_version_variables(${HIVE_VERSION})

# Set minimum OSX/iOS version
cu_setup_apple_minimum_versions(MACOS 10.13 IOS 11)

############ Define some variables

set(HIVE_RESOURCES_FOLDER "${CMAKE_CURRENT_LIST_DIR}/resources")

if(FALSE)
	set(INSTALL_TEST_FLAG "INSTALL")
endif()
if(${ENABLE_CODE_SIGNING})
	set(SIGN_FLAG "SIGN")
endif()

# Install paramaters
if(APPLE)
	set(MACOS_INSTALL_FOLDER "${PROJECT_NAME} ${CU_PROJECT_MARKETING_VERSION}")
	list(APPEND SDR_PARAMETERS "BUNDLE_DIR" "${MACOS_INSTALL_FOLDER}" "RUNTIME_DIR" "${MACOS_INSTALL_FOLDER}")
endif()
list(APPEND SDR_PARAMETERS QT_MAJOR_VERSION ${QT_MAJOR_VERSION})

############ Add projects

# Add Libraries
add_subdirectory(libs)

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

# Add CPack
if(ENABLE_HIVE_CPACK)
	add_subdirectory(installer)
endif()

# Force Project as visual default target
set_directory_properties(PROPERTIES VS_STARTUP_PROJECT ${PROJECT_NAME})

# Allow generator expressions in install(CODE/SCRIPT)
cmake_policy(SET CMP0087 NEW)
