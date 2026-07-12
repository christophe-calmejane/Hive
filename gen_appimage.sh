#!/usr/bin/env bash
# Useful script to generate an AppImage for Hive

################# PROJECT SPECIFIC VARIABLES
cmake_opt="-DBUILD_HIVE_TESTS=FALSE -DENABLE_HIVE_CPACK=FALSE -DENABLE_CODE_SIGNING=FALSE -DENABLE_HIVE_FEATURE_SPARKLE=FALSE"

# AppImage configuration
appimage_app_name="Hive"
appimage_executable="src/Hive"
appimage_icon="resources/Hive.png"
appimage_use_qt_plugin=1
appimage_categories="AudioVideo;Audio;Network;"
appimage_comment="ATDECC (IEEE Std 1722.1) controller for AVB/Milan networks"
appimage_mime_types="application/x-hive-ans;application/x-hive-ave;application/x-hive-hej;"
appimage_setcap_capability="cap_net_raw+ep"
declare -a appimage_additional_libs=()

############################ DO NOT MODIFY AFTER THAT LINE #############

# Get absolute folder for this script
selfFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/" # Command to get the absolute path

# Check if setup_fresh_env has been called
if [ ! -f "${selfFolderPath}.initialized" ]; then
	echo "ERROR: Please run setup_fresh_env.sh (just once) after having cloned this repository."
	exit 4
fi

# Include default values
if [ ! -f "${selfFolderPath}.defaults.sh" ]; then
	echo "ERROR: Missing ${selfFolderPath}.defaults.sh file"
	echo "Copy .defaults.sh.sample file to .defaults.sh then edit it to your needs."
	exit 4
fi
. "${selfFolderPath}.defaults.sh"

# Include utils functions
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/utils.sh"

# Only Linux supported
if ! isLinux; then
	echo "ERROR: AppImage generation is only supported on Linux"
	exit 4
fi

# Include config file extension
. "${selfFolderPath}extend_config_file.sh"

# Include config file functions
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/load_config_file.sh"

# Load config file
configFile=".hive_config"
loadConfigFile

# Parse Qt variables
overrideQtDir=0
QtDir=""
overrideQtVers=0
QtVersion="${default_qt_version}"

function extend_ga_fnc_help()
{
	local qtBaseInstallPath=""
	local qtArchName=""
	local default_path=""
	get_default_qt_path qtBaseInstallPath
	get_default_qt_arch qtArchName
	getQtDir default_path "${qtBaseInstallPath}" "${qtArchName}" "${QtVersion}"

	echo " -qtvers <Qt Version> -> Override the default Qt version (v${default_qt_version}) with the specified one."
	echo " -qtdir <Qt CMake Folder> -> Override default Qt path (${default_path}) with the specified one."
}

function extend_ga_fnc_unhandled_arg()
{
	case "$1" in
		-qtvers)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -qtvers option, see help (-h)"
				exit 4
			fi
			overrideQtVers=1
			QtVersion="$1"
			gen_cmake_additional_options+=("-qtvers")
			gen_cmake_additional_options+=("$1")
			return 2
			;;
		-qtdir)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -qtdir option, see help (-h)"
				exit 4
			fi
			overrideQtDir=1
			QtDir="$1"
			gen_cmake_additional_options+=("-qtdir")
			gen_cmake_additional_options+=("$1")
			return 2
			;;
	esac
	return 0
}

function extend_ga_fnc_postparse()
{
	# -qtvers is required if -qtdir is used
	if [[ ${overrideQtDir} -eq 1 && ${overrideQtVers} -eq 0 ]]; then
		echo "ERROR: -qtdir option requires -qtvers option, see help (-h)"
		exit 4
	fi
}

function extend_ga_fnc_setup_env()
{
	# Find libpcap and add it to additional libs
	local pcapLib=$(find /usr/lib -name "libpcap.so*" -type f 2>/dev/null | head -1)
	if [ -z "$pcapLib" ]; then
		pcapLib=$(find /usr/lib -name "libpcap.so*" -type l 2>/dev/null | head -1)
	fi
	if [ -n "$pcapLib" ]; then
		appimage_additional_libs+=("$pcapLib")
	else
		echo "WARNING: libpcap not found, it will not be bundled in the AppImage"
	fi

	# Resolve Qt directory
	if [ $overrideQtDir -eq 0 ]; then
		local qtBaseInstallPath=""
		local qtArchName=""
		get_default_qt_path qtBaseInstallPath
		get_default_qt_arch qtArchName
		getQtDir QtDir "${qtBaseInstallPath}" "${qtArchName}" "${QtVersion}"
	fi

	# Set QMAKE path for linuxdeploy-plugin-qt
	local qtMajorVersion="${QtVersion%%.*}"
	local qtBaseDir="${QtDir%/lib/cmake}"
	local qmakePath="${qtBaseDir}/bin/qmake${qtMajorVersion}"
	if [ ! -f "$qmakePath" ]; then
		qmakePath="${qtBaseDir}/bin/qmake"
	fi
	if [ -f "$qmakePath" ]; then
		export QMAKE="$qmakePath"
	else
		echo "WARNING: qmake not found at ${qtBaseDir}/bin/, linuxdeploy-plugin-qt may not find Qt correctly"
	fi

	# Add Qt lib dir to LD_LIBRARY_PATH
	local qtLibDir="${qtBaseDir}/lib"
	if [ -d "$qtLibDir" ]; then
		export LD_LIBRARY_PATH="${qtLibDir}:${LD_LIBRARY_PATH:-}"
	fi
}

function extend_ga_fnc_props_summary()
{
	echo "| - QT VERS: ${QtVersion}"
}

# Execute gen_appimage script from bashUtils
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/gen_appimage.sh"
