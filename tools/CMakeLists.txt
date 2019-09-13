# Hive Tools CMake File

######## MsgPack2Json
# Declare project
setup_project(msgPack2json "1.0" "Message Pack To JSON Converter")

add_executable(${PROJECT_NAME} msgPack2json.cpp)

# Setup common options
setup_executable_options(${PROJECT_NAME})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json)

# Sign binary (this is done during installation phase)
if(ENABLE_HIVE_SIGNING)
	sign_target(${PROJECT_NAME})
endif()

# Set installation rule (always installing application)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)


######## Json2MsgPack
# Declare project
setup_project(json2msgPack "1.0" "JSON To Message Pack Converter")

add_executable(${PROJECT_NAME} json2msgPack.cpp)

# Setup common options
setup_executable_options(${PROJECT_NAME})

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE nlohmann_json)

# Sign binary (this is done during installation phase)
if(ENABLE_HIVE_SIGNING)
	sign_target(${PROJECT_NAME})
endif()

# Set installation rule (always installing application)
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
