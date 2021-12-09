#!/usr/bin/env bash
# Useful script to generate project files using cmake

# Qt Version to use
qtVersion="5.15.2"

# Override default cmake options
cmake_opt="-DBUILD_HIVE_TESTS=TRUE -DENABLE_HIVE_CPACK=FALSE -DENABLE_CODE_SIGNING=FALSE -DENABLE_HIVE_FEATURE_SPARKLE=FALSE"

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

if [[ "x${params["use_sparkle"]}" == "xtrue" && $OSTYPE == darwin* ]]; then
	if [ ! -f "${selfFolderPath}resources/dsa_pub.pem" ]; then
		echo "ERROR: Please run setup_fresh_env.sh (just once) after having having changed use_sparkle value in config file."
		exit 4
	fi
	dsaPubKey="$(< "${selfFolderPath}resources/dsa_pub.pem")"
	cmake_opt="${cmake_opt} -DHIVE_DSA_PUB_KEY=${dsaPubKey} -DENABLE_HIVE_FEATURE_SPARKLE=TRUE"
fi

useSources=0
overrideQt5dir=0
Qt5dir=""

function extend_gc_fnc_help()
{
	echo " -source -> Use Qt source instead of precompiled binarie (Default: Not using sources). Do not use with -qt5dir option."
	echo " -qt5dir <Qt5 CMake Folder> -> Override automatic Qt5_DIR detection with the full path to the folder containing Qt5Config.cmake file (either binary or source)"
}

function extend_gc_fnc_unhandled_arg()
{
	case "$1" in
		-source)
			useSources=1
			return 1
			;;
		-qt5dir)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -qt5dir option, see help (-h)"
				exit 4
			fi
			overrideQt5dir=1
			Qt5dir="$1"
			return 2
			;;
	esac
	return 0
}

function extend_gc_fnc_postparse()
{
	if [[ $useSources -eq 1 && $overrideQt5dir -eq 1 ]]; then
		echo "ERROR: Cannot use -source and -qt5dir options at the same time."
		echo "If you want to use a custom source folder, just set -qt5dir to <Qt Source Root Folder>/qtbase/lib/cmake/Qt5"
		exit 4
	fi
}

function extend_gc_fnc_precmake()
{
	# Using automatic Qt5_DIR detection
	if [ $overrideQt5dir -eq 0 ]; then
		if isWindows; then
			qtBasePath="c:/Qt/${qtVersion}"
			if [ "$arch" == "x64" ]; then
				qtArch="msvc2017_64"
			else
				qtArch="msvc2019"
			fi
		elif isMac; then
			qtBasePath="/Applications/Qt/${qtVersion}"
			qtArch="clang_64"
		elif isLinux; then
			if [ "x${QT_BASE_PATH}" != "x" ]; then
				if [ ! -f "${QT_BASE_PATH}/MaintenanceTool" ]; then
					echo "Invalid QT_BASE_PATH: MaintenanceTool not found in specified folder: ${QT_BASE_PATH}"
					echo "Maybe try the -qt5dir option, see help (-h)"
					exit 1
				fi

				qtBasePath="${QT_BASE_PATH}/${qtVersion}"
				qtArch="gcc_64"
			else
				echo "Using cmake's auto-detection of Qt headers and libraries"
				echo "QT_BASE_PATH env variable can be defined to the root folder of Qt installation (where MaintenanceTool resides), or the -qt5dir option. See help (-h) for more details."
			fi
		else
			echo "Unsupported platform"
			exit 1
		fi

		# Check specified Qt version is available
		if [ ! -d "${qtBasePath}" ];
		then
			echo "Cannot find Qt v$qtVersion installation path."
			exit 1
		fi

		# Check specified Qt arch is available
		if [ ! -d "${qtBasePath}/${qtArch}" ]; then
			echo "Cannot find Qt arch '${qtArch}' for Qt v${qtVersion}"
			exit 1
		fi

		if [ $useSources -eq 1 ]; then
			# Override qtArch path with Source path
			qtArch="Src/qtbase"
			echo "Using Qt source instead of precompiled libraries"
		fi
		Qt5dir="${qtBasePath}/${qtArch}/lib/cmake/Qt5"
	fi

	# Validate Qt5dir
	if [ ! -f "${Qt5dir}/Qt5Config.cmake" ]; then
		echo "Invalid Qt5_DIR folder (${Qt5dir}): Qt5Config.cmake not found in the folder"
		exit 1
	fi

	add_cmake_opt+=("-DQt5_DIR=${Qt5dir}")
}

# execute gen_cmake script from bashUtils
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/gen_cmake.sh"
