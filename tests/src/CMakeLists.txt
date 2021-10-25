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
add_executable(Tests ${TESTS_SOURCE})

# Setup common options
cu_setup_executable_options(Tests)

# Set IDE folder
set_target_properties(Tests PROPERTIES FOLDER "Tests")

# Link with required libraries
target_link_libraries(Tests PRIVATE gtest ${PROJECT_NAME}_static)

# Set installation rule
if(INSTALL_HIVE_TESTS)
	install(TARGETS Tests RUNTIME CONFIGURATIONS Release DESTINATION bin)
endif()
