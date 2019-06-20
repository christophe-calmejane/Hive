############ CPack configuration

# Set Install Key (used to detect if a previous version should be uninstalled first)
set(HIVE_INSTALL_KEY "${PROJECT_NAME} ${HIVE_MARKETING_VERSION_STRING}")

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

# Basic settings
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "${PROJECT_COMPANYNAME}")
set(CPACK_PACKAGE_VERSION "${HIVE_FRIENDLY_VERSION_STRING}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_FULL_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_VENDOR}/${HIVE_INSTALL_KEY}")
if(NOT HIVE_INSTALLER_NAME)
	message(FATAL_ERROR "Variable HIVE_INSTALLER_NAME has not been set.")
endif()
set(CPACK_PACKAGE_FILE_NAME "${HIVE_INSTALLER_NAME}")
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_ROOT_DIR}/COPYING.LESSER")
set(CPACK_PACKAGE_ICON "${ICON_PATH}")

# Advanced settings
set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME};${PROJECT_NAME} ${HIVE_MARKETING_VERSION_STRING}")
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

		# NSIS Common settings
		set(CPACK_NSIS_COMPRESSOR "/SOLID LZMA")
		set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
		set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_FULL_NAME} ${HIVE_MARKETING_VERSION_STRING}") # Name to be shown in the title bar of the installer
		set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_NAME} ${HIVE_MARKETING_VERSION_STRING}") # Name to be shown in Windows Add/Remove Program control panel
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
		install(FILES ${PROJECT_ROOT_DIR}/resources/win32/WinPcap_4_1_3.exe DESTINATION . CONFIGURATIONS Release COMPONENT WinPcap)
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
			CreateShortCut \\\"$DESKTOP\\\\${PROJECT_NAME} ${HIVE_MARKETING_VERSION_STRING}.lnk\\\" \\\"$INSTDIR\\\\bin\\\\${PROJECT_NAME}.exe\\\" \\\"\\\""
		)

		# Remove shortcuts during uninstall
		set(CPACK_NSIS_DELETE_ICONS_EXTRA "\
			Delete \\\"$DESKTOP\\\\${PROJECT_NAME} ${HIVE_MARKETING_VERSION_STRING}.lnk\\\""
		)

		include(CPack REQUIRED)

		# Setup components
		cpack_add_component(Hive DISPLAY_NAME "${PROJECT_NAME}" DESCRIPTION "Installs ${PROJECT_NAME} Application." REQUIRED)
		cpack_add_component(WinPcap DISPLAY_NAME "WinPCap" DESCRIPTION "Installs WinPcap, necessary if not already installed on the system.")

	elseif(APPLE)

		set(CPACK_GENERATOR DragNDrop)

		set(CPACK_DMG_FORMAT UDBZ)
		# set(CPACK_DMG_DS_STORE "${PROJECT_ROOT_DIR}/resources/macOS/DS_Store")

		include(CPack REQUIRED)

	endif()

endif()
