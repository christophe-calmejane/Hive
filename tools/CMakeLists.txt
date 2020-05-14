# Hive Tools CMake File

######## AEM Dumper
add_subdirectory(AEMDumper)

######## MsgPack2Json
# Declare project
setup_project(msgPack2json "1.0" "Message Pack To JSON Converter")

add_executable(${PROJECT_NAME} msgPack2json.cpp)

# Setup common options
setup_executable_options(${PROJECT_NAME})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json)

# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
setup_deploy_runtime(${PROJECT_NAME} INSTALL ${SIGN_FLAG})

######## Json2MsgPack
# Declare project
setup_project(json2msgPack "1.0" "JSON To Message Pack Converter")

add_executable(${PROJECT_NAME} json2msgPack.cpp)

# Setup common options
setup_executable_options(${PROJECT_NAME})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json)

# Deploy and install target and its runtime dependencies (call this AFTER ALL dependencies have been added to the target)
setup_deploy_runtime(${PROJECT_NAME} INSTALL ${SIGN_FLAG})
