############ CPack configuration

# License file
set(CPACK_RESOURCE_FILE_LICENSE "${PROJECT_ROOT_DIR}/COPYING.LESSER")

# Add the required system libraries
set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)
include(InstallRequiredSystemLibraries)

# Basic settings
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VENDOR "${PROJECT_COMPANYNAME}")
set(CPACK_PACKAGE_VERSION "${HIVE_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_FULL_NAME}")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_VENDOR}/${PROJECT_NAME}")
if(HIVE_INSTALLER_NAME)
	set(CPACK_PACKAGE_FILE_NAME "${HIVE_INSTALLER_NAME}")
endif()

# Advanced settings
set(CPACK_PACKAGE_EXECUTABLES "${PROJECT_NAME};${PROJECT_FULL_NAME}")
set(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "${PROJECT_NAME}")
set(CPACK_CREATE_DESKTOP_LINKS "${PROJECT_NAME}")

# Platform-specific options
if(WIN32)
	# Install vcredist
	install(FILES ${PROJECT_ROOT_DIR}/resources/win32/vcredist_x86_v140.exe DESTINATION . CONFIGURATIONS Release)

	# Define common variables
	set(COMMON_RESOURCES_FOLDER "${PROJECT_ROOT_DIR}/resources/")
	set(WIN32_RESOURCES_FOLDER "${COMMON_RESOURCES_FOLDER}win32/")
	
	# Transform / to \ in paths
	string(REPLACE "/" "\\\\" COMMON_RESOURCES_FOLDER "${COMMON_RESOURCES_FOLDER}")
	string(REPLACE "/" "\\\\" WIN32_RESOURCES_FOLDER "${WIN32_RESOURCES_FOLDER}")
	string(REPLACE "/" "\\\\" CPACK_PACKAGE_INSTALL_DIRECTORY "${CPACK_PACKAGE_INSTALL_DIRECTORY}")

	# Common settings
	set(CPACK_PACKAGE_ICON "${WIN32_RESOURCES_FOLDER}Icon.ico")
	set(CPACK_NSIS_COMPRESSOR "/SOLID LZMA")
	set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
	set(CPACK_NSIS_PACKAGE_NAME "${PROJECT_FULL_NAME}") # Name to be shown in the title bar of the installer
	set(CPACK_NSIS_DISPLAY_NAME "${PROJECT_FULL_NAME}") # Name to be shown in Windows Add/Remove Program control panel
	set(CPACK_NSIS_INSTALLED_ICON_NAME "bin/${PROJECT_NAME}.exe") # Icon to be shown in Windows Add/Remove Program control panel
	set(CPACK_NSIS_HELP_LINK "${PROJECT_URL}")
	set(CPACK_NSIS_URL_INFO_ABOUT "${PROJECT_URL}")
	#set(CPACK_NSIS_CONTACT "${PROJECT_CONTACT}")
	
	# Visuals during installation and uninstallation
	set(CPACK_NSIS_MUI_ICON "${WIN32_RESOURCES_FOLDER}Icon.ico")
	set(CPACK_NSIS_MUI_UNIICON "${WIN32_RESOURCES_FOLDER}Icon.ico")
	set(CPACK_NSIS_MUI_WELCOMEFINISHPAGE_BITMAP "${WIN32_RESOURCES_FOLDER}Logo.bmp")
	set(CPACK_NSIS_MUI_UNWELCOMEFINISHPAGE_BITMAP "${WIN32_RESOURCES_FOLDER}Logo.bmp")
	set(CPACK_NSIS_INSTALLER_MUI_ICON_CODE "\
		!define MUI_WELCOMEFINISHPAGE_BITMAP_NOSTRETCH\n\
		!define MUI_UNWELCOMEFINISHPAGE_BITMAP_NOSTRETCH\n\
		!define MUI_WELCOMEPAGE_TITLE_3LINES\n\
	")
	
	# Extra install commands
	set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${CPACK_NSIS_EXTRA_INSTALL_COMMANDS}\n\
		; Install the VS2015 redistributables\n\
		ExecWait '\\\"$INSTDIR\\\\vcredist_x86_v140.exe\\\" /repair /quiet /norestart'\n\
		Delete \\\"$INSTDIR\\\\vcredist_x86_v140.exe\\\"\n\
		IfErrors -1\n\
		\n\
		; Write the size of the installation directory\n\
		!include \\\"FileFunc.nsh\\\"\n\
		\\\${GetSize} \\\"$INSTDIR\\\" \\\"/S=0K\\\" $0 $1 $2\n\
		\\\${GetSize} \\\"$INSTDIR\\\" \\\"/M=vcredist*.exe /S=0K\\\" $3 $1 $2\n\
		IntOp $0 $0 - $3 ;Remove the size of the VC++ redistributables\n\
		IntFmt $0 \\\"0x%08X\\\" $0\n\
		WriteRegDWORD SHCTX \\\"Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\${CPACK_PACKAGE_INSTALL_REGISTRY_KEY}\\\" \\\"EstimatedSize\\\" \\\"$0\\\""
	)
	
	# Add shortcuts during install
	set(CPACK_NSIS_CREATE_ICONS_EXTRA "\
		CreateShortCut \\\"$DESKTOP\\\\${PROJECT_NAME}.lnk\\\" \\\"$INSTDIR\\\\bin\\\\${PROJECT_NAME}.exe\\\" \\\"\\\""
	)
	
	# Remove shortcuts during uninstall
	set(CPACK_NSIS_DELETE_ICONS_EXTRA "\
		Delete \\\"$DESKTOP\\\\${PROJECT_NAME}.lnk\\\""
	)

elseif(APPLE)
	set(CPACK_PACKAGE_ICON "${PROJECT_ROOT_DIR}/resources/macOS/Icon.icns")
	set(CPACK_GENERATOR DragNDrop)
	set(CPACK_DMG_FORMAT UDBZ)
	# set(CPACK_DMG_DS_STORE "${PROJECT_ROOT_DIR}/resources/macOS/DS_Store")
endif()

# Lastly (now that all CPACK variables have been set), include CPack to generate the 'package' target
include(CPack)
