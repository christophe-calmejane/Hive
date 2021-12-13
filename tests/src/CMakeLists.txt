# Hive Unit Tests

# gtest include directories
include_directories(${gtest_SOURCE_DIR}/include)

# gtest link directories
link_directories(${gtest_BINARY_DIR}/src)

### Unit Tests
set(TESTS_SOURCE
	main.cpp
)

# Define target
add_executable(Tests MACOSX_BUNDLE ${TESTS_SOURCE})

if(ENABLE_HIVE_FEATURE_SPARKLE)
	fixup_sparkleHelper_dependencies() # Temporary fix to resolve SparkleHelper transitive dependencies
endif()

# Setup common options
cu_setup_executable_options(Tests)

# Set IDE folder
set_target_properties(Tests PROPERTIES FOLDER "Tests")

# Link with required libraries
target_link_libraries(Tests PRIVATE gtest ${PROJECT_NAME}_static)

# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
cu_setup_deploy_runtime(Tests ${INSTALL_TEST_FLAG} ${SIGN_FLAG} ${SDR_PARAMETERS})
