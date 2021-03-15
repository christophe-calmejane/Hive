############ CPack configuration

# Set Install Key (used to detect if a previous version should be uninstalled first)
set(HIVE_INSTALL_KEY "${PROJECT_NAME}")
if(HIVE_MARKETING_VERSION)
	string(APPEND HIVE_INSTALL_KEY " ${HIVE_MARKETING_VERSION}")
endif()

# Compute Install Version in the form 0xXXYYZZWW
math(EXPR HIVE_INSTALL_VERSION "0" OUTPUT_FORMAT HEXADECIMAL)
# Start with the first 3 digits
foreach(index RANGE 0 2)
	list(GET HIVE_VERSION_SPLIT ${index} LOOP_VERSION)
	math(EXPR HIVE_INSTALL_VERSION "${HIVE_INSTALL_VERSION} + (${LOOP_VERSION} << (8 * (3 - ${index})))" OUTPUT_FORMAT HEXADECIMAL)
endforeach()
# If the last digit is 0 (meaning release version), force it to greatest possible value
if(${HIVE_VERSION_BETA} STREQUAL "0")
	math(EXPR HIVE_INSTALL_VERSION "${HIVE_INSTALL_VERSION} + 0xFF" OUTPUT_FORMAT HEXADECIMAL)
else()
	math(EXPR HIVE_INSTALL_VERSION "${HIVE_INSTALL_VERSION} + ${HIVE_VERSION_BETA}" OUTPUT_FORMAT HEXADECIMAL)
endif()

# Use IFW on all platform instead of os-dependant installer
#set(USE_IFW_GENERATOR ON)

# Add the required system libraries
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)
include(InstallRequiredSystemLibraries)

# Define common variables
set(COMMON_RESOURCES_FOLDER "${PROJECT_ROOT_DIR}/resources/")
set(WIN32_RESOURCES_FOLDER "${COMMON_RESOURCES_FOLDER}win32/")
set(MACOS_RESOURCES_FOLDER "${COMMON_RESOURCES_FOLDER}macOS/")
if(WIN32)
	set(ICON_PATH "${WIN32_RESOURCES_FOLDER}Icon.ico")
elseif(APPLE)
	set(ICON_PATH "${MACOS_RESOURCES_FOLDER}Icon.icns")
endif()

# Define variables that include the Marketing version
if(HIVE_MARKETING_VERSION STREQUAL "")
	set(HIVE_NAME_AND_VERSION "${PROJECT_NAME}")
	set(HIVE_INSTALL_DISPLAY_NAME "${PROJECT_NAME} ${HIVE_FRIENDLY_VERSION}")
	set(HIVE_DOT_VERSION "")
else()
	set(HIVE_NAME_AND_VERSION "${PROJECT_NAME} ${HIVE_MARKETING_VERSION}")
	set(HIVE_INSTALL_DISPLAY_NAME "${PROJECT_NAME} ${HIVE_MARKETING_VERSION}")
	set(HIVE_DOT_VERSION ".${HIVE_MARKETING_VERSION}")
endif()

# Basic settings
set(CPACK_PACKAGE_NAME "${HIVE_NAME_AND_VERSION}")
set(CPACK_PACKAGE_VENDOR "${PROJECT_COMPANYNAME}")
set(CPACK_PACKAGE_VERSION "${HIVE_FRIENDLY_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_FULL_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_VENDOR}/${HIVE_INSTALL_KEY}")
if(NOT HIVE_INSTALLER_NAME)
	message(FATAL_ERROR "Variable HIVE_INSTALLER_NAME has not been set.")
endif()
set(CPACK_PACKAGE_FILE_NAME "${HIVE_INSTALLER_NAME}")
unset(CPACK_RESOURCE_FILE_README)
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_ROOT_DIR}/COPYING.LESSER.txt")
set(CPACK_PACKAGE_ICON "${ICON_PATH}")

# Advanced settings
set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME};${HIVE_NAME_AND_VERSION}")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${HIVE_INSTALL_KEY}")
set(CPACK_CREATE_DESKTOP_LINKS "${PROJECT_NAME}")

# Fix paths
if(WIN32)
	# Transform / to \ in paths
	string(REPLACE "/" "\\\\" COMMON_RESOURCES_FOLDER "${COMMON_RESOURCES_FOLDER}")
	string(REPLACE "/" "\\\\" WIN32_RESOURCES_FOLDER "${WIN32_RESOURCES_FOLDER}")
	string(REPLACE "/" "\\\\" CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_INSTALL_DIRECTORY}")
	string(REPLACE "/" "\\\\" CPACK_PACKAGE_ICON "${CPACK_PACKAGE_ICON}")
	string(REPLACE "/" "\\\\" ICON_PATH "${ICON_PATH}")
endif()

if(USE_IFW_GENERATOR)

	message(FATAL_ERROR "TODO")

else()

	# Platform-specific options
	if(WIN32)
		set(CPACK_GENERATOR NSIS)

		# Set CMake module path to our own nsis template is used during nsis generation
		set(CMAKE_MODULE_PATH ${PROJECT_ROOT_DIR}/installer/nsis ${CMAKE_MODULE_PATH})

		# Configure file with custom definitions for NSIS.
		configure_file(
			${PROJECT_ROOT_DIR}/installer/nsis/NSIS.definitions.nsh.in
			${LA_TOP_LEVEL_BINARY_DIR}/NSIS.definitions.nsh
		)

		# NSIS Common settings
		set(CPACK_NSIS_COMPRESSOR "/SOLID LZMA")
		set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
		set(CPACK_NSIS_PACKAGE_NAME "${CPACK_PACKAGE_NAME}") # Name to be shown in the title bar of the installer
		set(CPACK_NSIS_DISPLAY_NAME "${HIVE_INSTALL_DISPLAY_NAME}") # Name to be shown in Windows Add/Remove Program control panel
		set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/${PROJECT_NAME}.exe") # Icon to be shown in Windows Add/Remove Program control panel
		set(CPACK_NSIS_HELP_LINK "${PROJECT_URL}")
		set(CPACK_NSIS_URL_INFO_ABOUT "${PROJECT_URL}")
		set(CPACK_NSIS_CONTACT "${PROJECT_CONTACT}")

		# Visuals during installation and uninstallation (not using CPACK_NSIS_MUI_* variables as they do not work properly)
		set(CPACK_NSIS_INSTALLER_MUI_ICON_CODE "\
			!define MUI_ICON \\\"${ICON_PATH}\\\"\n\
			!define MUI_UNICON \\\"${ICON_PATH}\\\"\n\
			!define MUI_WELCOMEFINISHPAGE_BITMAP \\\"${WIN32_RESOURCES_FOLDER}Logo.bmp\\\"\n\
			!define MUI_UNWELCOMEFINISHPAGE_BITMAP \\\"${WIN32_RESOURCES_FOLDER}Logo.bmp\\\"\n\
			!define MUI_WELCOMEFINISHPAGE_BITMAP_NOSTRETCH\n\
			!define MUI_UNWELCOMEFINISHPAGE_BITMAP_NOSTRETCH\n\
			!define MUI_WELCOMEPAGE_TITLE_3LINES\n\
			!define MUI_STARTMENUPAGE_DEFAULTFOLDER \\\"${HIVE_INSTALL_KEY}\\\"\n\
			BrandingText \\\"${CPACK_PACKAGE_VENDOR} ${PROJECT_NAME}\\\"\n\
		")

		# Extra install commands
		install(FILES ${PROJECT_ROOT_DIR}/resources/win32/vc_redist.x86.exe DESTINATION . CONFIGURATIONS Release)
		set(COMPONENT_NAME_WINPCAP WinPcap)
		install(FILES ${PROJECT_ROOT_DIR}/resources/win32/WinPcap_4_1_3.exe DESTINATION . CONFIGURATIONS Release COMPONENT ${COMPONENT_NAME_WINPCAP})
		set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}\n\
			; Install the VS2017 redistributables\n\
			ExecWait '\\\"$INSTDIR\\\\vc_redist.x86.exe\\\" /repair /quiet /norestart'\n\
			Delete \\\"$INSTDIR\\\\vc_redist.x86.exe\\\"\n\
			IfErrors -1\n\
			\n\
			; Install WinPcap\n\
			ExecWait '\\\"$INSTDIR\\\\WinPcap_4_1_3.exe\\\"'\n\
			Delete \\\"$INSTDIR\\\\WinPcap_4_1_3.exe\\\"\n\
			IfErrors -1\n\
			\n\
			; Write the size of the installation directory\n\
			!include \\\"FileFunc.nsh\\\"\n\
			\\\${GetSize} \\\"$INSTDIR\\\" \\\"/S=0K\\\" $0 $1 $2\n\
			\\\${GetSize} \\\"$INSTDIR\\\" \\\"/M=vc_redist*.exe /S=0K\\\" $3 $1 $2\n\
			IntOp $0 $0 - $3 ;Remove the size of the VC++ redistributables\n\
			IntFmt $0 \\\"0x%08X\\\" $0\n\
			WriteRegDWORD SHCTX \\\"Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\${HIVE_INSTALL_KEY}\\\" \\\"EstimatedSize\\\" \\\"$0\\\""
		)

		# Add shortcuts during install
		set(CPACK_NSIS_CREATE_ICONS_EXTRA "\
			CreateShortCut \\\"$DESKTOP\\\\${HIVE_NAME_AND_VERSION}.lnk\\\" \\\"$INSTDIR\\\\bin\\\\${PROJECT_NAME}.exe\\\" \\\"\\\""
		)

		# Remove shortcuts during uninstall
		set(CPACK_NSIS_DELETE_ICONS_EXTRA "\
			Delete \\\"$DESKTOP\\\\${HIVE_NAME_AND_VERSION}.lnk\\\""
		)

		# Add a finish page to run the program
		set(CPACK_NSIS_MUI_FINISHPAGE_RUN "${PROJECT_NAME}.exe")

		# Include CPack so we can call cpack_add_component
		include(CPack REQUIRED)

		# Setup components
		cpack_add_component(${CMAKE_INSTALL_DEFAULT_COMPONENT_NAME} DISPLAY_NAME "${PROJECT_NAME}" DESCRIPTION "Installs ${PROJECT_NAME} Application." REQUIRED)
		cpack_add_component(${COMPONENT_NAME_WINPCAP} DISPLAY_NAME "WinPCap" DESCRIPTION "Installs WinPcap, necessary if not already installed on the system.")

	elseif(APPLE)

		set(CPACK_GENERATOR productbuild)

		# Set CMake module path to our own nsis template is used during nsis generation
		set(CMAKE_MODULE_PATH ${PROJECT_ROOT_DIR}/installer/productbuild ${CMAKE_MODULE_PATH})

		set(CPACK_PRODUCTBUILD_RESOURCES_DIR ${PROJECT_ROOT_DIR}/installer/productbuild/resources)
		set(CPACK_PRODUCTBUILD_BACKGROUND "background.png")
		set(CPACK_PRODUCTBUILD_BACKGROUND_ALIGNMENT "bottomleft")
		set(CPACK_PRODUCTBUILD_BACKGROUND_SCALING "proportional")
		set(CPACK_PRODUCTBUILD_BACKGROUND_MIME_TYPE "image/png")
		set(CPACK_PRODUCTBUILD_BACKGROUND_DARKAQUA "background.png")
		set(CPACK_PRODUCTBUILD_BACKGROUND_DARKAQUA_ALIGNMENT "bottomleft")
		set(CPACK_PRODUCTBUILD_BACKGROUND_DARKAQUA_SCALING "proportional")
		set(CPACK_PRODUCTBUILD_BACKGROUND_DARKAQUA_MIME_TYPE "image/png")
		set(CPACK_PRODUCTBUILD_IDENTITY_NAME "${LA_INSTALLER_SIGNING_IDENTITY}")
		set(CPACK_PKGBUILD_IDENTITY_NAME "${LA_INSTALLER_SIGNING_IDENTITY}")

		string(REGEX REPLACE "([][+.*()^])" "\\\\\\1" ESCAPED_IDENTITY "${LA_INSTALLER_SIGNING_IDENTITY}")

		# Create ChmodBPF install package
		set(INSTALL_CHMODBPF_GENERATED_PKG "${CMAKE_BINARY_DIR}/install.ChmodBPF.pkg")
		add_custom_command(OUTPUT "${INSTALL_CHMODBPF_GENERATED_PKG}"
			COMMAND find
				"${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/root"
				-type d
				-exec chmod 755 "{}" +
			COMMAND chmod 644
				"${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/root/Library/LaunchDaemons/com.KikiSoft.Hive.ChmodBPF.plist"
			COMMAND chmod 755
				"${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/root/Library/Application Support/Hive/ChmodBPF/ChmodBPF"
			COMMAND pkgbuild
				--identifier com.KikiSoft.Hive.ChmodBPF.pkg
				--version 1.1
				--preserve-xattr
				--root "${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/root"
				--sign ${ESCAPED_IDENTITY}
				--scripts "${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/install-scripts"
				${INSTALL_CHMODBPF_GENERATED_PKG}
			DEPENDS
				"${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/root/Library/Application Support/Hive/ChmodBPF/ChmodBPF"
				"${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/root/Library/LaunchDaemons/com.KikiSoft.Hive.ChmodBPF.plist"
				"${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/install-scripts/postinstall"
		)
		set(INSTALL_CHMODBPF_GENERATED_PRODUCT "${CMAKE_BINARY_DIR}/Install ChmodBPF.pkg")
		add_custom_command(OUTPUT "${INSTALL_CHMODBPF_GENERATED_PRODUCT}"
			COMMAND productbuild
			--identifier com.KikiSoft.Hive.ChmodBPF.product
			--version 1.1
			--sign ${ESCAPED_IDENTITY}
			--distribution "${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/install-distribution.xml"
			--package-path "${CMAKE_BINARY_DIR}"
			${INSTALL_CHMODBPF_GENERATED_PRODUCT}
		DEPENDS
			"${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/install-distribution.xml"
			${INSTALL_CHMODBPF_GENERATED_PKG}
		)
		add_custom_target(install_chmodbpf_pkg ALL DEPENDS "${INSTALL_CHMODBPF_GENERATED_PRODUCT}")
		install(PROGRAMS "${INSTALL_CHMODBPF_GENERATED_PRODUCT}" DESTINATION "${MACOS_INSTALL_FOLDER}" CONFIGURATIONS Release)

		# Create ChmodBPF uninstall package
		set(UNINSTALL_CHMODBPF_GENERATED_PKG "${CMAKE_BINARY_DIR}/uninstall.ChmodBPF.pkg")
		add_custom_command(OUTPUT "${UNINSTALL_CHMODBPF_GENERATED_PKG}"
			COMMAND pkgbuild
				--identifier com.KikiSoft.Hive.ChmodBPF.pkg
				--version 1.1
				--nopayload
				--sign ${ESCAPED_IDENTITY}
				--scripts "${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/uninstall-scripts"
				${UNINSTALL_CHMODBPF_GENERATED_PKG}
			DEPENDS
				"${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/uninstall-scripts/postinstall"
		)
		set(UNINSTALL_CHMODBPF_GENERATED_PRODUCT "${CMAKE_BINARY_DIR}/Uninstall ChmodBPF.pkg")
		add_custom_command(OUTPUT "${UNINSTALL_CHMODBPF_GENERATED_PRODUCT}"
			COMMAND productbuild
			--identifier com.KikiSoft.Hive.ChmodBPF.product
			--version 1.1
			--sign ${ESCAPED_IDENTITY}
			--distribution "${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/uninstall-distribution.xml"
			--package-path "${CMAKE_BINARY_DIR}"
			${UNINSTALL_CHMODBPF_GENERATED_PRODUCT}
		DEPENDS
			"${PROJECT_ROOT_DIR}/installer/productbuild/ChmodBPF/uninstall-distribution.xml"
			${UNINSTALL_CHMODBPF_GENERATED_PKG}
		)
		add_custom_target(uninstall_chmodbpf_pkg ALL DEPENDS "${UNINSTALL_CHMODBPF_GENERATED_PRODUCT}")
		install(PROGRAMS "${UNINSTALL_CHMODBPF_GENERATED_PRODUCT}" DESTINATION "${MACOS_INSTALL_FOLDER}" CONFIGURATIONS Release)

		# Create uninstall package
		configure_file(
			"${PROJECT_ROOT_DIR}/installer/productbuild/uninstaller/postinstall.in"
			"${CMAKE_BINARY_DIR}/uninstaller/install-scripts/postinstall"
		)
		set(UNINSTALL_HIVE_GENERATED_PKG "${CMAKE_BINARY_DIR}/uninstaller.pkg")
		add_custom_command(OUTPUT "${UNINSTALL_HIVE_GENERATED_PKG}"
			COMMAND pkgbuild
				--identifier com.KikiSoft.Hive.uninstaller.pkg
				--version 1.0
				--nopayload
				--sign ${ESCAPED_IDENTITY}
				--scripts "${CMAKE_BINARY_DIR}/uninstaller/install-scripts/"
				"${UNINSTALL_HIVE_GENERATED_PKG}"
			DEPENDS
				"${PROJECT_ROOT_DIR}/installer/productbuild/uninstaller/postinstall.in"
				"${CMAKE_BINARY_DIR}/uninstaller/install-scripts/postinstall"
		)
		set(UNINSTALL_HIVE_GENERATED_PRODUCT "${CMAKE_BINARY_DIR}/Uninstall ${CPACK_PACKAGE_NAME}.pkg")
		add_custom_command(OUTPUT "${UNINSTALL_HIVE_GENERATED_PRODUCT}"
			COMMAND productbuild
				--identifier com.KikiSoft.Hive.uninstaller.product
				--version 1.0
				--sign ${ESCAPED_IDENTITY}
				--distribution "${PROJECT_ROOT_DIR}/installer/productbuild/uninstaller/install-distribution.xml"
				--package-path "${CMAKE_BINARY_DIR}"
				${UNINSTALL_HIVE_GENERATED_PRODUCT}
			DEPENDS
				"${PROJECT_ROOT_DIR}/installer/productbuild/uninstaller/install-distribution.xml"
				${UNINSTALL_HIVE_GENERATED_PKG}
		)
		add_custom_target(uninstall_pkg ALL DEPENDS "${UNINSTALL_HIVE_GENERATED_PRODUCT}")
		install(PROGRAMS "${UNINSTALL_HIVE_GENERATED_PRODUCT}" DESTINATION "${MACOS_INSTALL_FOLDER}")

		# Include CPack so we can call cpack_add_component
		include(CPack REQUIRED)

		# Setup components
		cpack_add_component(${CMAKE_INSTALL_DEFAULT_COMPONENT_NAME} DISPLAY_NAME "${PROJECT_NAME}" DESCRIPTION "Installs ${PROJECT_NAME} Application." REQUIRED)

	endif()

endif()
