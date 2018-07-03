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
  
	echo -n "Creating symbolic link for dev branch... "
	local osName
	getOS osName
	if [[ $osName = win ]];
	then
		local absPath
		getFolderAbsoluteOSDependantPath absPath "3rdparty/"
		local log=$(cmd /C "mklink /J ${absPath}avdecc-local ${absPath}avdecc" 2>&1)
		if [ $? -ne 0 ];
		then
			echo "failed!"
			echo ""
			echo "Error log:"
			echo $log
			exit 1
		fi
	else
		local log=$(ln -sf 3rdparty/avdecc 3rdparty/avdecc-local 2>&1)
		if [ $? -ne 0 ];
		then
			echo "failed!"
			echo ""
			echo "Error log:"
			echo $log
			exit 1
		fi
	fi
	echo "done (feel free to change created '3rdparty/avdecc-local' symlink to something custom)"
	
	echo -n "Downloading WinPcap Developer's Pack... "
	local result
	result=$(which wget 2>&1)
	if [ $? -ne 0 ];
	then
		echo "failed, wget not found (see 3rdparty/avdecc/externals/3rdparty/winpcap/README.me for manually installation instructions)"
	else
		local outputFile="_WpdPack.zip"
		rm -f "$outputFile"
		local log=$("$result" https://www.winpcap.org/install/bin/WpdPack_4_1_2.zip -O "$outputFile" 2>&1)
		if [ $? -ne 0 ];
		then
			echo "failed!"
			echo ""
			echo "Error log:"
			echo $log
			rm -f "$outputFile"
			exit 1
		fi
		echo "done"
		
		echo -n "Installing WinPcap Developer's Pack... "
		result=$(which unzip 2>&1)
		if [ $? -ne 0 ];
		then
			echo "failed, unzip not found (see 3rdparty/avdecc/externals/3rdparty/winpcap/README.me for manually installation instructions)"
			rm -f "$outputFile"
		else
			local outputFolder="_WpdPack"
			rm -rf "$outputFolder"
			local log=$("$result" -d "$outputFolder" "$outputFile" 2>&1)
			if [ $? -ne 0 ];
			then
				echo "failed!"
				echo ""
				echo "Error log:"
				echo $log
				rm -f "$outputFile"
				rm -rf "$outputFolder"
				exit 1
			fi
			mv "${outputFolder}/WpdPack/Include" "3rdparty/avdecc/externals/3rdparty/winpcap/"
			mv "${outputFolder}/WpdPack/Lib" "3rdparty/avdecc/externals/3rdparty/winpcap/"
			rm -f "$outputFile"
			rm -rf "$outputFolder"
			echo "done"
		fi
	fi
}

setupEnv
echo "All done!"

exit 0
