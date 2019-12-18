#!/bin/bash

getFolderAbsoluteOSDependantPath()
{
  local _retval="$1"
  local result="$2"

  if [[ $OSTYPE = cygwin ]];
	then
    result="$(cygpath -a -w "$2")\\"
  elif [[ $OSTYPE = msys ]];
	then
    result="$({ cd "$2" && pwd -W; } | sed 's|/|\\|g')\\"
  else
    result="$(cd "$2"; pwd -P)/"
  fi

  eval $_retval="'${result}'"
}

getOS()
{
  local _retval="$1"
  local result=""

  case "$OSTYPE" in
    msys)
      result="win"
      ;;
    cygwin)
      result="win"
      ;;
    darwin*)
      result="mac"
      ;;
    linux*)
      result="linux"
      ;;
    *)
      echo "ERROR: Unknown OSTYPE: $OSTYPE"
      exit 127
      ;;
  esac

  eval $_retval="'${result}'"
}

setupEnv()
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

		local baseSparkleFolder="3rdparty/winsparkle"
		if [[ ! -d "${baseSparkleFolder}/bin" || ! -d "${baseSparkleFolder}/include" || ! -d "${baseSparkleFolder}/lib" ]];
		then
			echo -n "Downloading WinSparkle... "
			local result
			result=$(which wget 2>&1)
			if [ $? -ne 0 ];
			then
				echo "failed, wget not found (see ${baseSparkleFolder}/README.me for manually installation instructions)"
			else
				local wspkloutputFile="_WinSparkle.zip"
				rm -f "$wspkloutputFile"
				local spklVersion="0.6.0"
				local spklZipName="WinSparkle-${spklVersion}"
				local log=$("$result" https://github.com/vslavik/winsparkle/releases/download/v${spklVersion}/${spklZipName}.zip -O "$wspkloutputFile" 2>&1)
				if [ $? -ne 0 ];
				then
					echo "failed!"
					echo ""
					echo "Error log:"
					echo $log
					rm -f "$wspkloutputFile"
					exit 1
				fi
				echo "done"

				echo -n "Installing WinSparkle... "
				result=$(which unzip 2>&1)
				if [ $? -ne 0 ];
				then
					echo "failed, unzip not found (see ${baseSparkleFolder}/README.me for manually installation instructions)"
					rm -f "$wspkloutputFile"
				else
					local wspklOutputFolder="_WinSparkle"
					rm -rf "$wspklOutputFolder"
					local log=$("$result" -d "$wspklOutputFolder" "$wspkloutputFile" 2>&1)
					if [ $? -ne 0 ];
					then
						echo "failed!"
						echo ""
						echo "Error log:"
						echo $log
						rm -f "$wspkloutputFile"
						rm -rf "$wspklOutputFolder"
						exit 1
					fi
					mkdir -p "${baseSparkleFolder}/bin/x86"
					mkdir -p "${baseSparkleFolder}/bin/x64"
					mkdir -p "${baseSparkleFolder}/lib/x86"
					mkdir -p "${baseSparkleFolder}/lib/x64"
					mv "${wspklOutputFolder}/${spklZipName}/include" "${baseSparkleFolder}/"
					mv "${wspklOutputFolder}/${spklZipName}/Release/WinSparkle.dll" "${baseSparkleFolder}/bin/x86"
					mv "${wspklOutputFolder}/${spklZipName}/Release/WinSparkle.lib" "${baseSparkleFolder}/lib/x86"
					mv "${wspklOutputFolder}/${spklZipName}/x64/Release/WinSparkle.dll" "${baseSparkleFolder}/bin/x64"
					mv "${wspklOutputFolder}/${spklZipName}/x64/Release/WinSparkle.lib" "${baseSparkleFolder}/lib/x64"
					rm -f "$wspkloutputFile"
					rm -rf "$wspklOutputFolder"
					echo "done"
				fi
			fi
		fi

		echo -n "Generating DSA keys... "
		local dsa_params="resources/dsa_param.pem"
		local dsa_pub_key="resources/dsa_pub.pem"
		local dsa_priv_key="resources/dsa_priv.pem"
		if [ -f "$dsa_pub_key" ];
		then
			echo "already found in resources, not generating new ones"
		else
			result=$(which openssl 2>&1)
			if [ $? -ne 0 ];
			then
				echo "failed, openssl not found"
			else
				openssl dsaparam 1024 < /dev/random > "$dsa_params" 2>&1
				openssl gendsa -out "$dsa_priv_key" "$dsa_params" 2>&1
				openssl dsa -in "$dsa_priv_key" -pubout -out "$dsa_pub_key" 2>&1
				chmod 600 "$dsa_priv_key" 2>&1
				chmod 644 "$dsa_pub_key" 2>&1
				rm -f "$dsa_params" 2>&1
				echo "done"
			fi
		fi

	elif [[ $osName == "mac" ]];
	then
		echo -n "Downloading Sparkle... "
		local baseSparkleFolder="3rdparty/sparkle"
		local result
		result=$(which wget 2>&1)
		if [ $? -ne 0 ];
		then
			echo "failed, wget not found (see ${baseSparkleFolder}/README.me for manually installation instructions)"
		else
			local spkloutputFile="_Sparkle.tar.bz2"
			rm -f "$spkloutputFile"
			local spklVersion="1.22.0"
			local log=$("$result" https://github.com/sparkle-project/Sparkle/releases/download/${spklVersion}/Sparkle-${spklVersion}.tar.bz2 -O "$spkloutputFile" 2>&1)
			if [ $? -ne 0 ];
			then
				echo "failed!"
				echo ""
				echo "Error log:"
				echo $log
				rm -f "$spkloutputFile"
				exit 1
			fi
			echo "done"

			echo -n "Installing Sparkle... "
			result=$(which tar 2>&1)
			if [ $? -ne 0 ];
			then
				echo "failed, tar not found (see ${baseSparkleFolder}/README.me for manually installation instructions)"
				rm -f "$spkloutputFile"
			else
				local spklOutputFolder="_Sparkle"
				rm -rf "$spklOutputFolder"
				mkdir -p "$spklOutputFolder"
				local log=$("$result" xvjf "$spkloutputFile" --directory "$spklOutputFolder" 2>&1)
				if [ $? -ne 0 ];
				then
					echo "failed!"
					echo ""
					echo "Error log:"
					echo $log
					rm -f "$spkloutputFile"
					rm -rf "$spklOutputFolder"
					exit 1
				fi
				rm -rf "${baseSparkleFolder}/Sparkle.framework"
				mv -f "${spklOutputFolder}/Sparkle.framework" "${baseSparkleFolder}/"
				mv -f "${spklOutputFolder}/bin/generate_keys" "${baseSparkleFolder}/"
				mv -f "${spklOutputFolder}/bin/sign_update" "${baseSparkleFolder}/"
				rm -f "$spkloutputFile"
				rm -rf "$spklOutputFolder"
				echo "done"
			fi
		fi

		echo -n "Generating DSA keys... "
		local dsa_pub_key="resources/dsa_pub.pem"
		if [ -f "$dsa_pub_key" ];
		then
			echo "already found in resources, not generating new ones"
		else
			local generateKeys="3rdparty/sparkle/generate_keys"
			if [ ! -f "$generateKeys" ];
			then
				echo "failed, $generateKeys not found"
			else
				echo -n "Keychain access might be requested, accept it... "
				"$generateKeys" &> /dev/null
				# Run it a second time as only the second successfull run will print the public key
				local generateKeysResult="$("$generateKeys")"
				if [ `echo "$generateKeysResult" | wc -l` -ne 6 ];
				then
					echo "failed, unexpected result from $generateKeys command, have you accepted keychain access?"
					exit 1
				fi
				local ed25519PubKey="$(echo "$generateKeysResult" | head -n 6 | tail -n 1)"
				echo "$ed25519PubKey" > "$dsa_pub_key"
				echo "done"
			fi
		fi

	elif [[ $osName == "linux" ]];
	then
		local dsa_pub_key="resources/dsa_pub.pem"
		touch "$dsa_pub_key"

	fi

	echo -n "Copying .hive_config file... "
	if [ -f ".hive_config" ];
	then
		echo "already found, not overriding"
	else
		cp -f ".hive_config.sample" ".hive_config"
	fi
	echo "done"
}

setupEnv
echo "All done! Feel free to edit the .hive_config file to customize your installer."

exit 0
