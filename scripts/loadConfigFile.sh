#!/usr/bin/env bash

# Parse config file
configFile=".hive_config"
declare -A params=()
declare -a knownOptions=("identity" "signtool_options" "notarization_username" "notarization_password" "appcast_releases" "appcast_betas" "symbols_symstore_path" "symbols_windows_pdb_server_path" "symbols_macos_dsym_server_path")

# Default values
params["identity"]="-"
params["notarization_username"]=""
params["notarization_password"]="@keychain:AC_PASSWORD"
params["appcast_releases"]="https://localhost/hive/appcast-release.xml"
params["appcast_betas"]="https://localhost/hive/appcast-beta.xml"
params["signtool_options"]="/a /sm /q /fd sha256 /tr http://timestamp.digicert.com"

loadConfigFile()
{
	parseFile "${configFile}" knownOptions[@] params

	# Quick check for identity in keychain
	if isMac && [[ ! "${params["identity"]}" == "-" ]]; then
		security find-identity -v -p codesigning | grep "$identityString" &> /dev/null
		if [ $? -ne 0 ]; then
			echo "ERROR: Invalid identity value in '${configFile}' file (not found in keychain, or not valid for codesigning): $identityString"
			exit 1
		fi
	fi
}
