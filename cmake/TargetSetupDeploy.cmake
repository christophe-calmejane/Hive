function(target_list_link_libraries TARGET_NAME LIBRARY_DEPENDENCIES_OUTPUT QT_DEPENDENCIES_OUTPUT)
	# Skip interface libraries
	get_target_property(_TARGET_TYPE ${TARGET_NAME} TYPE)
	if(_TARGET_TYPE STREQUAL "INTERFACE_LIBRARY")
		return()
	endif()
	# Generate list of libraries on which the target depends
	list(APPEND _LIBRARIES "")
	get_target_property(_LINK_LIBRARIES ${TARGET_NAME} LINK_LIBRARIES)
	if(_LINK_LIBRARIES)
		list(APPEND _LIBRARIES ${_LINK_LIBRARIES})
	endif()
	get_target_property(_INTERFACE_LINK_LIBRARIES ${TARGET_NAME} INTERFACE_LINK_LIBRARIES)
	if(_INTERFACE_LINK_LIBRARIES)
		list(APPEND _LIBRARIES ${_INTERFACE_LINK_LIBRARIES})
	endif()
	if(_LIBRARIES)
		# Remove duplicates and the target itself
		list(REMOVE_DUPLICATES _LIBRARIES)
		list(REMOVE_ITEM _LIBRARIES ${TARGET_NAME})
		# Check dependencies
		foreach(_LIBRARY ${_LIBRARIES})
			if(${_LIBRARY} MATCHES "Qt5::")
				list(APPEND ${QT_DEPENDENCIES_OUTPUT} ${TARGET_NAME})
				continue()
			endif()
			if(TARGET ${_LIBRARY})
				get_target_property(_LIBRARY_TYPE ${_LIBRARY} TYPE)
				if(_LIBRARY_TYPE STREQUAL "SHARED_LIBRARY")
					list(APPEND ${LIBRARY_DEPENDENCIES_OUTPUT} ${_LIBRARY})
				endif()
				target_list_link_libraries(${_LIBRARY} ${LIBRARY_DEPENDENCIES_OUTPUT} ${QT_DEPENDENCIES_OUTPUT})
			endif()
		endforeach()
	endif()
	set(${LIBRARY_DEPENDENCIES_OUTPUT} ${${LIBRARY_DEPENDENCIES_OUTPUT}} PARENT_SCOPE)
	set(${QT_DEPENDENCIES_OUTPUT} ${${QT_DEPENDENCIES_OUTPUT}} PARENT_SCOPE)
endfunction()

function(target_setup_deploy TARGET_NAME)
	target_list_link_libraries(${TARGET_NAME} _LIBRARY_DEPENDENCIES_OUTPUT _QT_DEPENDENCIES_OUTPUT)

	# Nothing to deploy?
	if(NOT _LIBRARY_DEPENDENCIES_OUTPUT AND NOT _QT_DEPENDENCIES_OUTPUT)
		return()
	endif()

	get_target_property(_IS_BUNDLE ${TARGET_NAME} MACOSX_BUNDLE)

	# We generate a cmake script that will contain all the commands
	set(DEPLOY_SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/deploy.cmake)
	# We also generate a deploy cache file so we are able to check if the deploy command has already been called
	set(DEPLOY_STAMP ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/deploy.stamp)

	string(APPEND DEPLOY_SCRIPT_CONTENT
		"if(EXISTS \"${DEPLOY_STAMP}\")\n"
		"	return()\n"
		"endif()\n"
		"message(STATUS \"Deploying runtime dependencies..\")\n"
		"file(WRITE \"${DEPLOY_STAMP}\" \"1\")\n")

	cmake_parse_arguments(DEPLOY "INSTALL" "QML_DIR" "" ${ARGN})

	if(_LIBRARY_DEPENDENCIES_OUTPUT)
		list(REMOVE_DUPLICATES _LIBRARY_DEPENDENCIES_OUTPUT)
		foreach(_LIBRARY ${_LIBRARY_DEPENDENCIES_OUTPUT})
			if(DEPLOY_INSTALL)
				if(APPLE AND _IS_BUNDLE)
					install(
						FILES $<TARGET_FILE:${_LIBRARY}>
						DESTINATION ./$<TARGET_FILE_NAME:${TARGET_NAME}>.app/Contents/Frameworks)
				elseif(WIN32)
					install(
						FILES $<TARGET_FILE:${_LIBRARY}>
						DESTINATION bin)
				else()
					install(
						FILES $<TARGET_FILE:${_LIBRARY}>
						DESTINATION lib)
				endif()
			endif()
			if(APPLE AND _IS_BUNDLE)
				string(APPEND DEPLOY_SCRIPT_CONTENT
					"file(COPY \"$<TARGET_FILE:${_LIBRARY}>\" DESTINATION \"$<TARGET_BUNDLE_CONTENT_DIR:${TARGET_NAME}>/Frameworks/\")\n")
			elseif(WIN32)
				string(APPEND DEPLOY_SCRIPT_CONTENT
					"file(COPY \"$<TARGET_FILE:${_LIBRARY}>\" DESTINATION \"$<TARGET_FILE_DIR:${TARGET_NAME}>\")\n")
			else()
				string(APPEND DEPLOY_SCRIPT_CONTENT
					"file(COPY \"$<TARGET_FILE:${_LIBRARY}>\" DESTINATION \"$<TARGET_FILE_DIR:${TARGET_NAME}>/../lib\")\n")
			endif()
		endforeach()
	endif()

	if(_QT_DEPENDENCIES_OUTPUT)
		list(REMOVE_DUPLICATES _QT_DEPENDENCIES_OUTPUT)
		if(WIN32 OR APPLE)
			if(NOT TARGET Qt5::qmake)
				message(FATAL_ERROR "Cannot find Qt5::qmake")
			endif()

			get_target_property(_QMAKE_LOCATION Qt5::qmake IMPORTED_LOCATION)
			get_filename_component(_DEPLOYQT_DIR ${_QMAKE_LOCATION} DIRECTORY)

			if(WIN32)
				file(TO_CMAKE_PATH "${_DEPLOYQT_DIR}/windeployqt" DEPLOY_QT_COMMAND)
			elseif(APPLE)
				file(TO_CMAKE_PATH "${_DEPLOYQT_DIR}/macdeployqt" DEPLOY_QT_COMMAND)
			endif()

			if (NOT DEPLOY_QML_DIR)
				set(DEPLOY_QML_DIR ".")
			endif()

			if(WIN32)
				# We also need to run deploy on the target executable so add it at the end of the list
				list(REMOVE_ITEM _QT_DEPENDENCIES_OUTPUT ${TARGET_NAME})
				list(APPEND _QT_DEPENDENCIES_OUTPUT ${TARGET_NAME})

				# Each dependency may depend on specific Qt module so run deploy on each one
				foreach(_QT_DEPENDENCY ${_QT_DEPENDENCIES_OUTPUT})
					# Run deploy and in a specific directory
					string(APPEND DEPLOY_SCRIPT_CONTENT
						"execute_process(COMMAND \"${DEPLOY_QT_COMMAND}\" -verbose 0 --dir \"$<TARGET_FILE_DIR:${TARGET_NAME}>/_deployqt\" --no-patchqt -no-translations -no-system-d3d-compiler --no-compiler-runtime --no-webkit2 -no-angle --no-opengl-sw --qmldir \"${DEPLOY_QML_DIR}\" \"$<TARGET_FILE:${_QT_DEPENDENCY}>\")\n")
				endforeach()

				# Copy the deployed content next to the target binary
				string(APPEND DEPLOY_SCRIPT_CONTENT
					"execute_process(COMMAND \"${CMAKE_COMMAND}\" -E copy_directory \"$<TARGET_FILE_DIR:${TARGET_NAME}>/_deployqt\" \"$<TARGET_FILE_DIR:${TARGET_NAME}>\")\n")

				# Mark the deploy folder if required for install
				if(DEPLOY_INSTALL)
					install(DIRECTORY $<TARGET_FILE_DIR:${TARGET_NAME}>/_deployqt/ DESTINATION bin)
				endif()
			elseif(APPLE)
				if(NOT _IS_BUNDLE)
					message(WARNING "Cannot deploy Qt dependencies on non-bundle application target ${TARGET_NAME}")
				else()
					string(APPEND DEPLOY_SCRIPT_CONTENT
						"execute_process(COMMAND \"${DEPLOY_QT_COMMAND}\" \"$<TARGET_BUNDLE_DIR:${TARGET_NAME}>\" -verbose=0 -qmldir=${DEPLOY_QML_DIR} \"-codesign=${LA_TEAM_IDENTIFIER}\")\n"
						)
				endif()
			endif()
		endif()
	endif()

	file(GENERATE
		OUTPUT ${DEPLOY_SCRIPT}
		CONTENT ${DEPLOY_SCRIPT_CONTENT})

	# Create a target that will take care of the dependencies
	if(DEPLOY_INSTALL)
		add_custom_target(deploy_${TARGET_NAME} ALL
			COMMAND ${CMAKE_COMMAND} -E remove -f ${DEPLOY_STAMP}
			COMMAND ${CMAKE_COMMAND} -P ${DEPLOY_SCRIPT}
			DEPENDS ${TARGET_NAME})
	else()
		add_custom_target(deploy_${TARGET_NAME}
			COMMAND ${CMAKE_COMMAND} -E remove -f ${DEPLOY_STAMP}
			COMMAND ${CMAKE_COMMAND} -P ${DEPLOY_SCRIPT}
			DEPENDS ${TARGET_NAME})
	endif()

	# Finally, run the deploy script as POST_BUILD command on the target
	add_custom_command(TARGET ${TARGET_NAME}
		POST_BUILD
		COMMAND ${CMAKE_COMMAND} -P ${DEPLOY_SCRIPT})
endfunction()
