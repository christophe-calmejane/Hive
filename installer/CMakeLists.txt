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
	set(COMPONENT_NAME_WINPCAP WinPcap)
	install(FILES "${CU_ROOT_DIR}/installer/win32/WinPcap_4_1_3.exe" DESTINATION . CONFIGURATIONS Release COMPONENT ${COMPONENT_NAME_WINPCAP})
	set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}\n\
		; Install WinPcap\n\
		ExecWait '\\\"$INSTDIR\\\\WinPcap_4_1_3.exe\\\"'\n\
		Delete \\\"$INSTDIR\\\\WinPcap_4_1_3.exe\\\"\n"
	)
endmacro()

# Declare a function that will be called before generating the NSIS package, to include custom components
macro(configure_NSIS_extra_components)
	cpack_add_component(${COMPONENT_NAME_WINPCAP} DISPLAY_NAME "WinPCap" DESCRIPTION "Installs WinPcap, necessary if not already installed on the system.")
endmacro()

# Declare a function that will be called before generating the ProductBuild package, to include custom commands
macro(configure_PRODUCTBUILD_extra_commands)
	set(CPACK_PRODUCTBUILD_RESOURCES_DIR ${CU_ROOT_DIR}/installer/productbuild/resources)

	# Create ChmodBPF install package
	set(CHMODBPF_NAME "ChmodBPF")
	set(INSTALL_CHMODBPF_GENERATED_PKG "${CMAKE_BINARY_DIR}/install.${CHMODBPF_NAME}.pkg")
	add_custom_command(OUTPUT "${INSTALL_CHMODBPF_GENERATED_PKG}"
		COMMAND find
			"${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/root"
			-type d
			-exec chmod 755 "{}" +
		COMMAND chmod 644
			"${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/root/Library/LaunchDaemons/${CU_COMPANY_DOMAIN}.${CU_PROJECT_COMPANYNAME}.${PROJECT_NAME}.${CHMODBPF_NAME}.plist"
		COMMAND chmod 755
			"${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/root/Library/Application Support/${PROJECT_NAME}/${CHMODBPF_NAME}/${CHMODBPF_NAME}"
		COMMAND pkgbuild
			--identifier ${CU_COMPANY_DOMAIN}.${CU_PROJECT_COMPANYNAME}.${PROJECT_NAME}.${CHMODBPF_NAME}.pkg
			--version 1.1
			--preserve-xattr
			--root "${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/root"
			--sign ${ESCAPED_IDENTITY}
			--scripts "${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/install-scripts"
			${INSTALL_CHMODBPF_GENERATED_PKG}
		DEPENDS
			"${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/root/Library/Application Support/${PROJECT_NAME}/${CHMODBPF_NAME}/${CHMODBPF_NAME}"
			"${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/root/Library/LaunchDaemons/${CU_COMPANY_DOMAIN}.${CU_PROJECT_COMPANYNAME}.${PROJECT_NAME}.${CHMODBPF_NAME}.plist"
			"${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/install-scripts/postinstall"
	)
	set(INSTALL_CHMODBPF_GENERATED_PRODUCT "${CMAKE_BINARY_DIR}/Install ${CHMODBPF_NAME}.pkg")
	add_custom_command(OUTPUT "${INSTALL_CHMODBPF_GENERATED_PRODUCT}"
		COMMAND productbuild
		--identifier ${CU_COMPANY_DOMAIN}.${CU_PROJECT_COMPANYNAME}.${PROJECT_NAME}.${CHMODBPF_NAME}.product
		--version 1.1
		--sign ${ESCAPED_IDENTITY}
		--distribution "${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/install-distribution.xml"
		--package-path "${CMAKE_BINARY_DIR}"
		${INSTALL_CHMODBPF_GENERATED_PRODUCT}
	DEPENDS
		"${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/install-distribution.xml"
		${INSTALL_CHMODBPF_GENERATED_PKG}
	)
	add_custom_target(install_chmodbpf_pkg ALL DEPENDS "${INSTALL_CHMODBPF_GENERATED_PRODUCT}")
	install(PROGRAMS "${INSTALL_CHMODBPF_GENERATED_PRODUCT}" DESTINATION "${MACOS_INSTALL_FOLDER}" CONFIGURATIONS Release)

	# Create ChmodBPF uninstall package
	set(UNINSTALL_CHMODBPF_GENERATED_PKG "${CMAKE_BINARY_DIR}/uninstall.${CHMODBPF_NAME}.pkg")
	add_custom_command(OUTPUT "${UNINSTALL_CHMODBPF_GENERATED_PKG}"
		COMMAND pkgbuild
			--identifier ${CU_COMPANY_DOMAIN}.${CU_PROJECT_COMPANYNAME}.${PROJECT_NAME}.${CHMODBPF_NAME}.pkg
			--version 1.1
			--nopayload
			--sign ${ESCAPED_IDENTITY}
			--scripts "${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/uninstall-scripts"
			${UNINSTALL_CHMODBPF_GENERATED_PKG}
		DEPENDS
			"${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/uninstall-scripts/postinstall"
	)
	set(UNINSTALL_CHMODBPF_GENERATED_PRODUCT "${CMAKE_BINARY_DIR}/Uninstall ${CHMODBPF_NAME}.pkg")
	add_custom_command(OUTPUT "${UNINSTALL_CHMODBPF_GENERATED_PRODUCT}"
		COMMAND productbuild
		--identifier ${CU_COMPANY_DOMAIN}.${CU_PROJECT_COMPANYNAME}.${PROJECT_NAME}.${CHMODBPF_NAME}.product
		--version 1.1
		--sign ${ESCAPED_IDENTITY}
		--distribution "${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/uninstall-distribution.xml"
		--package-path "${CMAKE_BINARY_DIR}"
		${UNINSTALL_CHMODBPF_GENERATED_PRODUCT}
	DEPENDS
		"${CU_ROOT_DIR}/installer/productbuild/${CHMODBPF_NAME}/uninstall-distribution.xml"
		${UNINSTALL_CHMODBPF_GENERATED_PKG}
	)
	add_custom_target(uninstall_chmodbpf_pkg ALL DEPENDS "${UNINSTALL_CHMODBPF_GENERATED_PRODUCT}")
	install(PROGRAMS "${UNINSTALL_CHMODBPF_GENERATED_PRODUCT}" DESTINATION "${MACOS_INSTALL_FOLDER}" CONFIGURATIONS Release)
endmacro()

# Declare a function that will be called before generating the NSIS package, to include custom components
macro(configure_PRODUCTBUILD_extra_components)
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
