# Hive Installer CMake file

# Set Install Key (used to detect if a previous version should be uninstalled first)
set(CU_INSTALL_KEY "${PROJECT_NAME}")
if(CU_PROJECT_MARKETING_VERSION)
	string(APPEND CU_INSTALL_KEY " ${CU_PROJECT_MARKETING_VERSION}")
endif()

# Use IFW on all platform instead of os-dependant installer
#set(USE_IFW_GENERATOR ON)

# Declare a function that will be called before generating the NSIS package, to include custom commands
macro(configure_NSIS_extra_commands)
	# Install vcredist
	install(FILES "${CU_ROOT_DIR}/installer/win32/vc_redist.x86.exe" DESTINATION . CONFIGURATIONS Release)
	set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}\n\
		; Install the VS2017 redistributables\n\
		ExecWait '\\\"$INSTDIR\\\\vc_redist.x86.exe\\\" /repair /quiet /norestart'\n\
		Delete \\\"$INSTDIR\\\\vc_redist.x86.exe\\\"\n\
		IfErrors -1\n\
		\n\
		; Write the size of the installation directory\n\
		!include \\\"FileFunc.nsh\\\"\n\
		\\\${GetSize} \\\"$INSTDIR\\\" \\\"/S=0K\\\" $0 $1 $2\n\
		\\\${GetSize} \\\"$INSTDIR\\\" \\\"/M=vc_redist*.exe /S=0K\\\" $3 $1 $2\n\
		IntOp $0 $0 - $3 ;Remove the size of the VC++ redistributables\n\
		IntFmt $0 \\\"0x%08X\\\" $0\n\
		WriteRegDWORD SHCTX \\\"Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\${PACKAGE_INSTALL_KEY}\\\" \\\"EstimatedSize\\\" \\\"$0\\\""
	)


	# Install winpcap
	install(FILES "${CU_ROOT_DIR}/installer/win32/WinPcap_4_1_3.exe" DESTINATION . CONFIGURATIONS Release)
	set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}\n\
		; Install WinPcap\n\
		ExecWait '\\\"$INSTDIR\\\\WinPcap_4_1_3.exe\\\"'\n\
		Delete \\\"$INSTDIR\\\\WinPcap_4_1_3.exe\\\"\n"
	)
endmacro()

# Declare a function that will be called before generating the ProductBuild package, to include custom commands
macro(configure_PRODUCTBUILD_extra_commands)
	set(CPACK_PRODUCTBUILD_RESOURCES_DIR ${CU_ROOT_DIR}/installer/productbuild/resources)
endmacro()

set(CU_INSTALL_LICENSE_FILE_PATH "${CU_ROOT_DIR}/COPYING.LESSER.txt")
if(WIN32)
	set(CU_INSTALL_ICO_FILE_PATH "${CU_ROOT_DIR}/resources/win32/Icon.ico")
elseif(APPLE)
	set(CU_INSTALL_ICO_FILE_PATH "${CU_ROOT_DIR}/resources/macOS/Icon.icns")
endif()

# Include common installer settings
include(${CU_ROOT_DIR}/3rdparty/avdecc/cmake/cmakeUtils/cpack/CPackConfig.cmake)
