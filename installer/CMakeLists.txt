# Hive Installer CMake file

# We must redefine the variables to take marketing digits/postfix into account (didn't do it before because cu_setup_project_version_variables is a macro and it would have defined the variables for all targets)
cu_setup_project_version_variables(${HIVE_VERSION} MARKETING_VERSION_DIGITS ${MARKETING_VERSION_DIGITS} MARKETING_VERSION_POSTFIX ${MARKETING_VERSION_POSTFIX})

# Set Install Key (used to detect if a previous version should be uninstalled first)
set(CU_INSTALL_KEY "${PROJECT_NAME}")
if(CU_PROJECT_MARKETING_VERSION)
	string(APPEND CU_INSTALL_KEY " ${CU_PROJECT_MARKETING_VERSION}")
endif()

include(${CU_BASE_DIR}/cpack/productbuild/ChmodBPF.cmake REQUIRED)

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
	set(COMPONENT_NAME_WINPCAP WinPcap)
	install(FILES "${CU_ROOT_DIR}/installer/win32/WinPcap_4_1_3.exe" DESTINATION . CONFIGURATIONS Release COMPONENT ${COMPONENT_NAME_WINPCAP})
	set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}\n\
		; Install WinPcap\n\
		ExecWait '\\\"$INSTDIR\\\\WinPcap_4_1_3.exe\\\"'\n\
		Delete \\\"$INSTDIR\\\\WinPcap_4_1_3.exe\\\"\n\
		# IfErrors -1\n"
	)
endmacro()

# Declare a function that will be called before generating the NSIS package, to include custom components
macro(configure_NSIS_extra_components)
	cpack_add_component(${COMPONENT_NAME_WINPCAP} DISPLAY_NAME "WinPCap" DESCRIPTION "Installs WinPcap, necessary if not already installed on the system.")
endmacro()

# Declare a function that will be called before generating the ProductBuild package, to include custom commands
macro(configure_PRODUCTBUILD_extra_commands)
	set(CPACK_PRODUCTBUILD_RESOURCES_DIR ${CU_ROOT_DIR}/installer/productbuild/resources)
	cu_chmodbpf_extra_commands()
endmacro()

# Declare a function that will be called before generating the ProductBuild package, to include custom components
macro(configure_PRODUCTBUILD_extra_components)
	cu_chmodbpf_add_component()
endmacro()

# Declare a function that will be called before generating the IFW package, to include custom components
macro(configure_IFW_extra_components)
	if(WIN32)
		# Setup a component for VC Redist
		set(VCREDIST_COMPONENT "VC_Redist")
		install(FILES "${CU_ROOT_DIR}/installer/win32/vc_redist.x86.exe" "${CU_ROOT_DIR}/installer/win32/run_vcredist.bat" DESTINATION . CONFIGURATIONS Release COMPONENT ${VCREDIST_COMPONENT})
		cpack_add_component(${VCREDIST_COMPONENT} DISPLAY_NAME "Microsoft Visual C++ 2017 Redistribuable" DESCRIPTION "Installs Microsoft Visual C++ 2017 Redistribuable" REQUIRED HIDDEN)
		cpack_ifw_configure_component(${VCREDIST_COMPONENT} SCRIPT "${CU_ROOT_DIR}/installer/ifw/vcredist.qs" SORTING_PRIORITY 0 FORCED_INSTALLATION)
		# Setup a component for WinPcap
		set(WINPCAP_COMPONENT "WinPcap")
		install(FILES "${CU_ROOT_DIR}/installer/win32/WinPcap_4_1_3.exe" "${CU_ROOT_DIR}/installer/win32/run_winpcap.bat" DESTINATION . CONFIGURATIONS Release COMPONENT ${WINPCAP_COMPONENT})
		cpack_add_component(${WINPCAP_COMPONENT} DISPLAY_NAME "WinPcap" DESCRIPTION "Installs WinPcap.\nRequired if not already installed on the system.")
		cpack_ifw_configure_component(${WINPCAP_COMPONENT} SCRIPT "${CU_ROOT_DIR}/installer/ifw/winpcap.qs" SORTING_PRIORITY 5)
	endif()
endmacro()

set(CU_INSTALL_LICENSE_FILE_PATH "${CU_ROOT_DIR}/COPYING.LESSER.txt")
if(WIN32)
	set(CU_INSTALL_ICO_FILE_PATH "${CU_ROOT_DIR}/resources/win32/Icon.ico")
	set(CU_INSTALL_NSIS_HEADER_FILE_PATH "${CU_ROOT_DIR}/installer/nsis/header.bmp")
	set(CU_INSTALL_NSIS_WELCOME_FILE_PATH "${CU_ROOT_DIR}/installer/nsis/welcome.bmp")
elseif(APPLE)
	set(CU_INSTALL_PRODUCTBUILD_BACKGROUND_FILE_PATH "background.png")
endif()

# Include common installer settings
include(${CU_ROOT_DIR}/3rdparty/avdecc/cmake/cmakeUtils/cpack/CPackConfig.cmake)
