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
		echo -n "Downloading WinPcap Developer's Pack... "
		local result
		result=$(which wget 2>&1)
		if [ $? -ne 0 ];
		then
			echo "failed, wget not found (see 3rdparty/avdecc/externals/3rdparty/winpcap/README.me for manually installation instructions)"
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
				echo "failed, unzip not found (see 3rdparty/avdecc/externals/3rdparty/winpcap/README.me for manually installation instructions)"
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
				mv "${wpcapOutputFolder}/WpdPack/Include" "3rdparty/avdecc/externals/3rdparty/winpcap/"
				mv "${wpcapOutputFolder}/WpdPack/Lib" "3rdparty/avdecc/externals/3rdparty/winpcap/"
				rm -f "$wpcapOutputFile"
				rm -rf "$wpcapOutputFolder"
				echo "done"
			fi
		fi

		echo -n "Downloading WinSparkle... "
		local baseSparkleFolder="3rdparty/winsparkle"
		local result
		result=$(which wget 2>&1)
		if [ $? -ne 0 ];
		then
			echo "failed, wget not found (see ${baseSparkleFolder}/README.me for manually installation instructions)"
		else
			local wspkloutputFile="_WinSparkle.zip"
			rm -f "$wspkloutputFile"
			local spklVersion="WinSparkle-0.6.0"
			local log=$("$result" https://github.com/vslavik/winsparkle/releases/download/v0.6.0/${spklVersion}.zip -O "$wspkloutputFile" 2>&1)
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
				mv "${wspklOutputFolder}/${spklVersion}/include" "${baseSparkleFolder}/"
				mv "${wspklOutputFolder}/${spklVersion}/Release/WinSparkle.dll" "${baseSparkleFolder}/bin/x86"
				mv "${wspklOutputFolder}/${spklVersion}/Release/WinSparkle.lib" "${baseSparkleFolder}/lib/x86"
				mv "${wspklOutputFolder}/${spklVersion}/x64/Release/WinSparkle.dll" "${baseSparkleFolder}/bin/x64"
				mv "${wspklOutputFolder}/${spklVersion}/x64/Release/WinSparkle.lib" "${baseSparkleFolder}/lib/x64"
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
			openssl gendsa "$dsa_params" -out "$dsa_priv_key" 2>&1
			openssl dsa -in "$dsa_priv_key" -pubout -out "$dsa_pub_key" 2>&1
			chmod 600 "$dsa_priv_key" 2>&1
			chmod 644 "$dsa_pub_key" 2>&1
			rm -f "$dsa_params" 2>&1
			echo "done"
		fi
	fi
}

setupEnv
echo "All done!"

exit 0
