#!/usr/bin/env bash
# Useful script to generate project files using cmake

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

# Include default values
if [ ! -f "${selfFolderPath}.defaults.sh" ]; then
	echo "ERROR: Missing ${selfFolderPath}.defaults.sh file"
	echo "Copy .defaults.sh.sample file to .defaults.sh then edit it to your needs."
	exit 4
fi
. "${selfFolderPath}.defaults.sh"

# Include utils functions
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/utils.sh"

# Include config file extension
. "${selfFolderPath}extend_config_file.sh"

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
useSparkle=0
dsaFilePath="${selfFolderPath}resources/dsa_pub.pem"

# Override defaults using config file
if [[ "x${params["use_sparkle"]}" == "xtrue" ]]; then
	useSparkle=1
fi

function extend_gc_fnc_help()
{
	local qtBaseInstallPath=""
	local qtArchName=""
	local default_path=""
	get_default_qt_path qtBaseInstallPath
	get_default_qt_arch qtArchName
	getQtDir default_path "${qtBaseInstallPath}" "${qtArchName}" "${QtVersion}"

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
	if [ $useSparkle -eq 1 ]; then
		add_cmake_opt+=("-DENABLE_HIVE_FEATURE_SPARKLE=TRUE")
		if [[ $OSTYPE == darwin* ]]; then
			if [ ! -f "${dsaFilePath}" ]; then
				echo "ERROR: Please run setup_fresh_env.sh (just once) after having having changed use_sparkle value in config file."
				exit 4
			fi
			dsaPubKey="$(< "${dsaFilePath}")"
			add_cmake_opt+=("-DHIVE_DSA_PUB_KEY=${dsaPubKey}")
		fi
	fi

	# -qtvers is required if -qtdir is used
	if [[ ${overrideQtDir} -eq 1 && ${overrideQtVers} -eq 0 ]]; then
		echo "ERROR: -qtdir option requires -qtvers option, see help (-h)"
		exit 4
	fi
}

function extend_gc_fnc_precmake()
{
	# Using automatic Qt detection
	if [ $overrideQtDir -eq 0 ]; then
		local qtBaseInstallPath=""
		local qtArchName=""
		get_default_qt_path qtBaseInstallPath
		get_default_qt_arch qtArchName
		getQtDir QtDir "${qtBaseInstallPath}" "${qtArchName}" "${QtVersion}"
	fi

	# Validate QtDir
	local qtMajorVersion="${QtVersion%%.*}"
	local qtConfigFileName="Qt${qtMajorVersion}Config.cmake"

	if [ ! -f "${QtDir}/Qt${qtMajorVersion}/${qtConfigFileName}" ]; then
		echo "Invalid Qt folder (${QtDir}): ${qtConfigFileName} not found in the folder."
		echo "Maybe try the -qtdir option, see help (-h)"
		exit 1
	fi

	add_cmake_opt+=("-DCMAKE_PREFIX_PATH=${QtDir}")
	add_cmake_opt+=("-DQT_MAJOR_VERSION=${qtMajorVersion}")

	# Add NewsFeeds
	add_cmake_opt+=("-DNEWSFEED_URL=${params["newsfeed_url"]}")
}

function extend_gc_fnc_props_summary()
{
	echo "| - QT VERS: ${QtVersion}"
}

# execute gen_cmake script from bashUtils
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/gen_cmake.sh"
