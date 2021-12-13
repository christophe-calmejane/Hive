#!/usr/bin/env bash
# Useful script to generate installer

################# PROJECT SPECIFIC VARIABLES
cmake_opt="-DBUILD_HIVE_TESTS=FALSE -DENABLE_HIVE_CPACK=TRUE -DENABLE_CODE_SIGNING=TRUE -DENABLE_HIVE_FEATURE_SPARKLE=FALSE"

# Change the default key_digits value
#default_keyDigits=0

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

use_sparkle=0
if [ "x${params["use_sparkle"]}" == "xtrue" ]; then
	use_sparkle=1
fi

if [[ $use_sparkle -eq 1 && $OSTYPE == darwin* ]]; then
	if [ ! -f "${selfFolderPath}resources/dsa_pub.pem" ]; then
		echo "ERROR: Please run setup_fresh_env.sh (just once) after having having changed use_sparkle value in config file."
		exit 4
	fi
	dsaPubKey="$(< "${selfFolderPath}resources/dsa_pub.pem")"
	cmake_opt="${cmake_opt} -DHIVE_DSA_PUB_KEY=${dsaPubKey} -DENABLE_HIVE_FEATURE_SPARKLE=TRUE"
fi

do_notarize=1
do_appcast=$use_sparkle
function extend_gi_fnc_help()
{
	if isMac; then
		echo " -no-notarize -> Do not notarize installer (Default: Do notarize)"
	fi
	if [ $use_sparkle -eq 1 ]; then
		echo " -no-appcast -> Do not generate appcast (Default: Do appcast)"
	fi
}

function extend_gi_fnc_unhandled_arg()
{
	case "$1" in
		-no-notarize)
			do_notarize=0
			return 1
			;;
		-no-appcast)
			if [ $use_sparkle -eq 1 ]; then
				do_appcast=0
				return 1
			fi
			return 0
			;;
	esac
	return 0
}

# execute gen_install script from bashUtils
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/gen_install.sh"
# Get path again, might be altered by previous call
selfFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/" # Command to get the absolute path

appcastInstallerName="${fullInstallerName}"

# macOS specific code
if isMac; then
	# Call notarization
	if [ $do_notarize -eq 1 ]; then
		if [ ! "x${params["notarization_username"]}" == "x" ]; then
			"${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/notarize_binary.sh" "${fullInstallerName}" "${params["notarization_username"]}" "${params["notarization_password"]}" "com.KikiSoft.Hive.${installerExtension}"
			if [ $? -ne 0 ]; then
				echo "Failed to notarize installer"
				exit 1
			fi
			# Get path again, might be altered by previous call
			selfFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/" # Command to get the absolute path
		fi
	fi

	# Tar the installer as Sparkle do not support PKG
	appcastInstallerName="${fullInstallerName}.tar"
	tar cvf "${appcastInstallerName}" "${fullInstallerName}" &> /dev/null
	rm -f "${fullInstallerName}" &> /dev/null
fi

# Generate appcast
if [ $do_appcast -eq 1 ]; then
	appcastURL="${params["appcast_releases"]}"
	installerSubUrl="release"
	if [ $is_release -eq 0 ];
	then
		appcastURL="${params["appcast_betas"]}"
		installerSubUrl="beta"
	fi
	# Get URL of folder containing appcast file
	updateBaseURL="${appcastURL%/*}/"

	# Generate appcast file
	"${selfFolderPath}3rdparty/sparkleHelper/scripts/generate_appcast.sh" "${appcastInstallerName}" "${releaseVersion}${beta_tag}" "${internalVersion}" "resources/dsa_priv.pem" "${updateBaseURL}changelog.php?lastKnownVersion=next" "${updateBaseURL}${installerSubUrl}/${appcastInstallerName}" "/S /NOPCAP /LAUNCH"

	echo ""
	echo "Do not forget to upload:"
	echo " - CHANGELOG.MD"
	echo " - Installer file: ${appcastInstallerName}"
	echo " - Updated appcast file(s)"
fi

exit 0
