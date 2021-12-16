#!/usr/bin/env bash
# Useful script to generate project files using cmake

# Override default cmake options
cmake_opt="-DBUILD_HIVE_TESTS=TRUE -DENABLE_HIVE_CPACK=FALSE -DENABLE_CODE_SIGNING=FALSE -DENABLE_HIVE_FEATURE_SPARKLE=FALSE"

# Default values
default_qt_version="5.15.2"
default_win_basePath="C:/Qt"
default_win_arch="msvc2019"
default_mac_basePath="/Applications/Qt"
default_mac_arch="clang_64"
default_linux_basePath="/usr/lib"
default_linux_arch="$(g++ -dumpmachine)"

############################ DO NOT MODIFY AFTER THAT LINE #############

# Get absolute folder for this script
selfFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/" # Command to get the absolute path

# Check if setup_fresh_env has been called
if [ ! -f "${selfFolderPath}.initialized" ]; then
	echo "ERROR: Please run setup_fresh_env.sh (just once) after having cloned this repository."
	exit 4
fi

# Include utils functions
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/utils.sh"

# Include config file functions
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/load_config_file.sh"

# Load config file
configFile=".hive_config"
loadConfigFile

if isLinux; then
	which g++ &> /dev/null
	if [ $? -ne 0 ];
	then
		echo "ERROR: g++ not found"
		exit 4
	fi
fi

# Parse variables
overrideQtDir=0
QtDir=""
overrideQtVers=0
QtVersion="${default_qt_version}"
QtMajorVersion="${default_qt_version%%.*}"
QtConfigFileName="Qt${QtMajorVersion}Config.cmake"
useSparkle=0
dsaFilePath="${selfFolderPath}resources/dsa_pub.pem"

# Override defaults using config file
if [[ "x${params["use_sparkle"]}" == "xtrue" ]]; then
	useSparkle=1
fi

function build_qt_config_folder()
{
	local _retval="$1"
	local basePath="$2"
	local arch="$3"
	local majorVers="$4"
	local result=""

	if isWindows; then
		result="${basePath}/${arch}/lib/cmake"
	elif isMac; then
		result="${basePath}/${arch}/lib/cmake"
	elif isLinux; then
		which g++ &> /dev/null
		if [ $? -ne 0 ];
		then
			echo "ERROR: g++ not found"
			exit 4
		fi
		result="${basePath}/${arch}/cmake"
	fi

	eval $_retval="'${result}'"
}

function extend_gc_fnc_help()
{
	local default_path=""
	if isWindows; then
		build_qt_config_folder default_path "${default_win_basePath}/${default_qt_version}" "${default_win_arch}" "${QtMajorVersion}"
	elif isMac; then
		build_qt_config_folder default_path "${default_mac_basePath}/${default_qt_version}" "${default_mac_arch}" "${QtMajorVersion}"
	elif isLinux; then
		build_qt_config_folder default_path "${default_linux_basePath}" "${default_linux_arch}" "${QtMajorVersion}"
	fi

	echo " -no-sparkle -> Disable sparkle usage, even if specified in config file"
	echo " -qtvers <Qt Version> -> Override the default Qt version (v${default_qt_version}) with the specified one."
	echo " -qtdir <Qt CMake Folder> -> Override default Qt path (${default_path}) with the specified one."
	echo "Note: You can also use the -qtdir option to use Qt source instead of precompiled binaries (specify 'Src/qtbase' instead of binary arch)."
}

function extend_gc_fnc_unhandled_arg()
{
	case "$1" in
		-no-sparkle)
			useSparkle=0
			return 1
			;;
		-qtvers)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -qtvers option, see help (-h)"
				exit 4
			fi
			overrideQtVers=1
			QtVersion="$1"
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
			return 2
			;;
	esac
	return 0
}

function extend_gc_fnc_postparse()
{
	# Get DSA public key (macOS needs it in the plist)
	if [[ $useSparkle -eq 1 && $OSTYPE == darwin* ]]; then
		if [ ! -f "${dsaFilePath}" ]; then
			echo "ERROR: Please run setup_fresh_env.sh (just once) after having having changed use_sparkle value in config file."
			exit 4
		fi
		dsaPubKey="$(< "${dsaFilePath}")"
		add_cmake_opt+=("-DHIVE_DSA_PUB_KEY=${dsaPubKey}")
		add_cmake_opt+=("-DENABLE_HIVE_FEATURE_SPARKLE=TRUE")
	fi

	# -qtvers is required if -qtdir is used
	if [[ ${overrideQtDir} -eq 1 && ${overrideQtVers} -eq 0 ]]; then
		echo "ERROR: -qtdir option requires -qtvers option, see help (-h)"
		exit 4
	fi
	# Reset QtMajorVersion and QtConfigFileName, since QtVersion might have been overridden by -qtvers option
	QtMajorVersion="${QtVersion%%.*}"
	QtConfigFileName="Qt${QtMajorVersion}Config.cmake"
}

function extend_gc_fnc_precmake()
{
	# Using automatic Qt detection
	if [ $overrideQtDir -eq 0 ]; then
		if isWindows; then
			qtBasePath="${default_win_basePath}/${QtVersion}"
			if [ "$arch" == "x64" ]; then
				qtArch="${default_win_arch}_64"
			else
				qtArch="${default_win_arch}"
			fi
		elif isMac; then
			qtBasePath="${default_mac_basePath}/${QtVersion}"
			if [ "${QtMajorVersion}" == "6" ] ; then
				qtArch="macos"
			else
				qtArch="${default_mac_arch}"
			fi
		elif isLinux; then
			if [ "x${QT_BASE_PATH}" != "x" ]; then
				if [ ! -f "${QT_BASE_PATH}/MaintenanceTool" ]; then
					echo "Invalid QT_BASE_PATH: MaintenanceTool not found in specified folder: ${QT_BASE_PATH}"
					echo "Maybe try the -qtdir option, see help (-h)"
					exit 1
				fi

				qtBasePath="${QT_BASE_PATH}/${QtVersion}"
				qtArch="" # Maybe use default_linux_arch as well? (if yes, factorize qtArch for both QT_BASE_PATH and system wide)
			else
				echo "Using system wide Qt headers and libraries."
				echo "QT_BASE_PATH env variable can be defined to the root folder of Qt installation (where MaintenanceTool resides), or the -qtdir option. See help (-h) for more details."
				qtBasePath="${default_linux_basePath}"
				qtArch="${default_linux_arch}"
			fi
		else
			echo "Unsupported platform"
			exit 1
		fi

		build_qt_config_folder QtDir "${qtBasePath}" "${qtArch}" "${QtMajorVersion}"
	fi

	# Validate QtDir
	if [ ! -f "${QtDir}/Qt${QtMajorVersion}/${QtConfigFileName}" ]; then
		echo "Invalid Qt folder (${QtDir}): ${QtConfigFileName} not found in the folder."
		echo "Maybe try the -qtdir option, see help (-h)"
		exit 1
	fi

	add_cmake_opt+=("-DCMAKE_PREFIX_PATH=${QtDir}")
	add_cmake_opt+=("-DQT_MAJOR_VERSION=${QtMajorVersion}")
}

function extend_gc_fnc_props_summary()
{
	echo "| - QT VERS: ${QtVersion}"
}

# execute gen_cmake script from bashUtils
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/gen_cmake.sh"
