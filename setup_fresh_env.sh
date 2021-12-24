#!/usr/bin/env bash

# Get absolute folder for this script
callerFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/" # Command to get the absolute path

setupSubmodules()
{
	echo -n "Fetching submodules... "
	local log=$(git submodule update --init --recursive 2>&1)
	if [ $? -ne 0 ];
	then
		echo "failed!"
		echo ""
		echo "Error log:"
		echo $log
		exit 1
	fi
	echo "done"
}

setupSparkleHelper()
{
	local sparkleHelperFolder="${callerFolderPath}3rdparty/sparkleHelper/"
	local sparkleHelperScriptsFolder="${sparkleHelperFolder}scripts/"
	local resourcesFolder="${callerFolderPath}resources"

	# Call setup_fresh_env.sh from SparkleHelper
	${sparkleHelperScriptsFolder}setup_fresh_env.sh "${params['identity']}"

	# Generate DSA keys
	${sparkleHelperScriptsFolder}generate_dsa_keys.sh "${resourcesFolder}"
}

setupPCap()
{
	getOS osName
	if [[ $osName == "win" ]];
	then
		local wpcapInstallFolder="3rdparty/avdecc/externals/3rdparty/winpcap"
		if [[ ! -d "${wpcapInstallFolder}/Include" || ! -d "${wpcapInstallFolder}/Lib" ]];
		then
			echo -n "Downloading WinPcap Developer's Pack... "
			local result
			result=$(which wget 2>&1)
			if [ $? -ne 0 ];
			then
				echo "failed, wget not found (see ${wpcapInstallFolder}/README.me for manually installation instructions)"
			else
				local wpcapOutputFile="_WpdPack.zip"
				rm -f "$wpcapOutputFile"
				local log=$("$result" https://www.winpcap.org/install/bin/WpdPack_4_1_2.zip -O "$wpcapOutputFile" 2>&1)
				if [ $? -ne 0 ];
				then
					echo "failed!"
					echo ""
					echo "Error log:"
					echo $log
					rm -f "$wpcapOutputFile"
					exit 1
				fi
				echo "done"
				
				echo -n "Installing WinPcap Developer's Pack... "
				result=$(which unzip 2>&1)
				if [ $? -ne 0 ];
				then
					echo "failed, unzip not found (see ${wpcapInstallFolder}/README.me for manually installation instructions)"
					rm -f "$wpcapOutputFile"
				else
					local wpcapOutputFolder="_WpdPack"
					rm -rf "$wpcapOutputFolder"
					local log=$("$result" -d "$wpcapOutputFolder" "$wpcapOutputFile" 2>&1)
					if [ $? -ne 0 ];
					then
						echo "failed!"
						echo ""
						echo "Error log:"
						echo $log
						rm -f "$wpcapOutputFile"
						rm -rf "$wpcapOutputFolder"
						exit 1
					fi
					mv "${wpcapOutputFolder}/WpdPack/Include" "${wpcapInstallFolder}/"
					mv "${wpcapOutputFolder}/WpdPack/Lib" "${wpcapInstallFolder}/"
					rm -f "$wpcapOutputFile"
					rm -rf "$wpcapOutputFolder"
					echo "done"
				fi
			fi
		fi
	fi

	echo "done"
}

configFile=".hive_config"
if [ ! -f "${configFile}" ]; then
	echo "ERROR: You must create and configure a ${configFile} file before running this script:"
	echo "Copy ${configFile}.sample file to ${configFile} then edit it to your needs."
	exit 1
fi

setupSubmodules

# Include utils functions
. "${callerFolderPath}3rdparty/avdecc/scripts/bashUtils/utils.sh"

# Include config file functions
. "${callerFolderPath}3rdparty/avdecc/scripts/bashUtils/load_config_file.sh"

# Load config file
loadConfigFile

if [ "x${params["use_sparkle"]}" == "xtrue" ]; then
	# Setup Sparkle
	setupSparkleHelper
fi

# Setup PCap
setupPCap

echo "DO NOT REMOVE THIS FILE" > ".initialized"

echo "All done!"

exit 0
