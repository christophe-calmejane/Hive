function(target_list_transitive_interface_link_libraries TARGET_NAME OUTPUT)
	get_target_property(_interface_link_libraries ${TARGET_NAME} INTERFACE_LINK_LIBRARIES)
	foreach(_library ${_interface_link_libraries})
		if(NOT TARGET ${_library})
			set(REGEXP "([A-Za-z0-9_]+)::([A-Za-z0-9_]+)")
			string(REGEX REPLACE ${REGEXP} "\\1" _package ${_library})
			string(REGEX REPLACE ${REGEXP} "\\2" _component ${_library})
			find_package(${_package} COMPONENTS ${_component} QUIET)
		endif()
		if(TARGET ${_library})
			get_target_property(_library_type ${_library} TYPE)
			if(_library_type STREQUAL "SHARED_LIBRARY")
				list(APPEND ${OUTPUT} ${_library})
			endif()
			target_list_transitive_interface_link_libraries(${_library} ${OUTPUT})
		endif()
	endforeach()
	set(${OUTPUT} ${${OUTPUT}} PARENT_SCOPE)
endfunction()

function(target_copy_library TARGET_NAME LIBRARY_TARGET_NAME)
	cmake_parse_arguments(COPY "INSTALL" "" "" ${ARGN})
	if(WIN32)
		add_custom_command(
			TARGET ${TARGET_NAME}
			POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${TARGET_NAME}>
			COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:${LIBRARY_TARGET_NAME}> $<TARGET_FILE_DIR:${TARGET_NAME}>)
		if(COPY_INSTALL)
			install(
				FILES $<TARGET_FILE:${LIBRARY_TARGET_NAME}>
				DESTINATION bin)
		endif()
	elseif(APPLE)
		if(COPY_INSTALL)
			get_target_property(_is_bundle ${TARGET_NAME} MACOSX_BUNDLE)
			if(_is_bundle)
				install(
					FILES $<TARGET_FILE:${LIBRARY_TARGET_NAME}>
					DESTINATION $<TARGET_BUNDLE_DIR:${TARGET_NAME}>/Contents/Frameworks)
			else()
				install(
					FILES $<TARGET_FILE:${LIBRARY_TARGET_NAME}>
					DESTINATION lib)
			endif()
		endif()
	endif()
endfunction()

function(target_copy_libraries TARGET_NAME)
	target_list_transitive_interface_link_libraries(${TARGET_NAME} _libraries)
	if(_libraries)
		list(REMOVE_DUPLICATES _libraries)
		foreach(_library ${_libraries})
			# Do not copy Qt5 libarries, the deploy script will do it properly
			if(NOT ${_library} MATCHES "Qt5::")
				target_copy_library(${TARGET_NAME} ${_library} ${ARGN})
			endif()
		endforeach()
	endif()
endfunction()
