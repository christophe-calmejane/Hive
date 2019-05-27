#!/bin/bash
# Useful script to generate project files using cmake

# Get absolute folder for this script
selfFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/" # Command to get the absolute path

# Include util functions
. "${selfFolderPath}3rdparty/avdecc/scripts/utils.sh"

# Override default cmake options
cmake_opt="-DENABLE_HIVE_CPACK=FALSE -DENABLE_HIVE_SIGNING=FALSE"

############################ DO NOT MODIFY AFTER THAT LINE #############

qtVersion="5.12.1"

# Default values
default_VisualGenerator="Visual Studio 15 2017"
default_VisualGeneratorArch="Win32"
default_VisualToolset="v141"
default_VisualToolchain="x64"
default_VisualArch="x86"
default_VisualSdk="8.1"

# 
cmake_generator=""
generator_arch=""
if isMac; then
	cmake_path="/Applications/CMake.app/Contents/bin/cmake"
	# CMake.app not found, use cmake from the path
	if [ ! -f "${cmake_path}" ]; then
		cmake_path="cmake"
	fi
	generator="Xcode"
	getCcArch arch
else
	# Use cmake from the path
	cmake_path="cmake"
	if isWindows; then
		generator="$default_VisualGenerator"
		generator_arch="$default_VisualGeneratorArch"
		toolset="$default_VisualToolset"
		toolchain="$default_VisualToolchain"
		platformSdk="$default_VisualSdk"
		arch="$default_VisualArch"
	else
		generator="Unix Makefiles"
		getCcArch arch
	fi
fi

which "${cmake_path}" &> /dev/null
if [ $? -ne 0 ]; then
	echo "CMake not found. Please add CMake binary folder in your PATH environment variable."
	exit 1
fi

outputFolder="./_build"
cmake_config=""
add_cmake_opt=()
outputFolderForced=0
useVSclang=0
useVS2019=0
hasTeamId=0
doSign=0
useSources=0

while [ $# -gt 0 ]
do
	case "$1" in
		-h)
			echo "Usage: gen_cmake.sh [options]"
			echo " -h -> Display this help"
			echo " -o <folder> -> Output folder (Default: ${outputFolder}_${arch})"
			echo " -f <flags> -> Force all cmake flags (Default: $cmake_opt)"
			echo " -a <flags> -> Add cmake flags to default ones (or to forced ones with -f option)"
			echo " -b <cmake path> -> Force cmake binary path (Default: $cmake_path)"
			echo " -c <cmake generator> -> Force cmake generator (Default: $generator)"
			if isWindows; then
				echo " -t <visual toolset> -> Force visual toolset (Default: $toolset)"
				echo " -tc <visual toolchain> -> Force visual toolchain (Default: $toolchain)"
				echo " -64 -> Generate the 64 bits version of the project (Default: 32)"
				echo " -vs2019 -> Compile using VS 2019 compiler instead of the default one"
				echo " -clang -> Compile using clang for VisualStudio (if predefined toolset do not work, override with -t option INSTEAD of -clang)"
			fi
			if isMac; then
				echo " -id <TeamIdentifier> -> iTunes team identifier for binary signing."
			fi
			if isLinux; then
				echo " -debug -> Force debug configuration (Linux only)"
				echo " -release -> Force release configuration (Linux only)"
			fi
			echo " -sign -> Sign binaries (Default: No signing)"
			echo " -source -> Use Qt source instead of precompiled binarie (Default: Not using sources)"
			exit 3
			;;
		-o)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -o option, see help (-h)"
				exit 4
			fi
			outputFolder="$1"
			outputFolderForced=1
			;;
		-f)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -f option, see help (-h)"
				exit 4
			fi
			cmake_opt="$1"
			;;
		-a)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -a option, see help (-h)"
				exit 4
			fi
			add_cmake_opt+=("$1")
			;;
		-b)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -b option, see help (-h)"
				exit 4
			fi
			if [ ! -x "$1" ]; then
				echo "ERROR: Specified cmake binary is not valid (not found or not executable): $1"
				exit 4
			fi
			cmake_path="$1"
			;;
		-c)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -c option, see help (-h)"
				exit 4
			fi
			cmake_generator="$1"
			;;
		-t)
			if isWindows; then
				shift
				if [ $# -lt 1 ]; then
					echo "ERROR: Missing parameter for -t option, see help (-h)"
					exit 4
				fi
				toolset="$1"
			else
				echo "ERROR: -t option is only supported on Windows platform"
				exit 4
			fi
			;;
		-tc)
			if isWindows; then
				shift
				if [ $# -lt 1 ]; then
					echo "ERROR: Missing parameter for -tc option, see help (-h)"
					exit 4
				fi
				toolchain="$1"
			else
				echo "ERROR: -tc option is only supported on Windows platform"
				exit 4
			fi
			;;
		-64)
			if isWindows; then
				generator_arch="x64"
				arch="x64"
			else
				echo "ERROR: -64 option is only supported on Windows platform (linux and macOS are already defaulting to 64 bits)"
				exit 4
			fi
			;;
		-vs2019)
			if isWindows; then
				useVS2019=1
			else
				echo "ERROR: -vs2019 option is only supported on Windows platform"
				exit 4
			fi
			;;
		-clang)
			if isWindows; then
				useVSclang=1
			else
				echo "ERROR: -clang option is only supported on Windows platform"
				exit 4
			fi
			;;
		-id)
			if isMac; then
				shift
				if [ $# -lt 1 ]; then
					echo "ERROR: Missing parameter for -id option, see help (-h)"
					exit 4
				fi
				add_cmake_opt+=("-DLA_TEAM_IDENTIFIER=$1")
				hasTeamId=1
			else
				echo "ERROR: -id option is only supported on macOS platform"
				exit 4
			fi
			;;
		-debug)
			if isLinux; then
				cmake_config="-DCMAKE_BUILD_TYPE=Debug"
			else
				echo "ERROR: -debug option is only supported on Linux platform"
				exit 4
			fi
			;;
		-release)
			if isLinux; then
				cmake_config="-DCMAKE_BUILD_TYPE=Release"
			else
				echo "ERROR: -release option is only supported on Linux platform"
				exit 4
			fi
			;;
		-sign)
			add_cmake_opt+=("-DENABLE_HIVE_SIGNING=TRUE")
			doSign=1
			;;
		-source)
			useSources=1
			;;
		*)
			echo "ERROR: Unknown option '$1' (use -h for help)"
			exit 4
			;;
	esac
	shift
done

if [ $outputFolderForced -eq 0 ]; then
	outputFolder="${outputFolder}_${arch}"
fi

if [ ! -z "$cmake_generator" ]; then
	echo "Overriding default cmake generator ($generator) with: $cmake_generator"
	generator="$cmake_generator"
fi

# Check TeamIdentifier specified is signing enabled on macOS
if isMac; then
	if [[ $hasTeamId -eq 0 && $doSign -eq 1 ]]; then
		echo "ERROR: macOS requires either iTunes TeamIdentifier to be specified using -id option, or -no-signing to disable binary signing"
		exit 4
	fi
fi

# Check if at least a -debug or -release option has been passed on linux
if isLinux; then
	if [ -z $cmake_config ]; then
		echo "ERROR: Linux requires either -debug or -release option to be specified"
		exit 4
	fi
fi

# Using -vs2019 option
if [ $useVS2019 -eq 1 ]; then
	generator="Visual Studio 16 2019"
	toolset="v142"
fi

# Using -clang option (shortcut to auto-define the toolset)
if [ $useVSclang -eq 1 ]; then
	toolset="v141_clang_c2"
fi

# Clang on windows does not properly compile using Sdk8.1, we have to force Sdk10.0
shopt -s nocasematch
if [[ isWindows && $toolset =~ clang ]]; then
	platformSdk="10.0"
fi

generator_arch_option=""
if [ ! -z "${generator_arch}" ]; then
	generator_arch_option="-A${generator_arch} "
fi

toolset_option=""
if [ ! -z "${toolset}" ]; then
	if [ ! -z "${toolchain}" ]; then
		toolset_option="-T${toolset},host=${toolchain} "
	else
		toolset_option="-T${toolset} "
	fi
fi

sdk_option=""
if [ ! -z "${platformSdk}" ]; then
	sdk_option="-DCMAKE_SYSTEM_VERSION=$platformSdk"
fi

hiveVersion=$(grep "HIVE_VERSION" CMakeLists.txt | perl -nle 'print $& if m{VERSION[ ]+\K[^ )]+}')
if [[ $hiveVersion == "" ]]; then
	echo "Cannot detect project version"
	exit 1
fi
hiveVersion="${hiveVersion//[$'\t\r\n']}"

# Check if we have a release or devel version
oldIFS="$IFS"
IFS='.' read -a versionSplit <<< "$hiveVersion"
IFS="$oldIFS"

# Check for legacy 'avdecc-local' folder
if [ -d "3rdparty/avdecc-local" ]; then
	echo "Legacy '3rdparty/avdecc-local' no longer used, please remove this folder/link"
	exit 1
fi
add_cmake_opt+=("-DAVDECC_BASE_FOLDER=3rdparty/avdecc")

if isWindows; then
	qtBasePath="c:/Qt/${qtVersion}"
	if [ "$arch" == "x64" ]; then
		qtArch="msvc2017_64"
	else
		qtArch="msvc2017"
	fi
elif isMac; then
	qtBasePath="/Applications/Qt/${qtVersion}"
	qtArch="clang_64"
elif isLinux; then
	if [ "x${QT_BASE_PATH}" != "x" ]; then
		if [ ! -f "${QT_BASE_PATH}/MaintenanceTool" ]; then
			echo "Invalid QT_BASE_PATH: MaintenanceTool not found in specified folder: ${QT_BASE_PATH}"
			exit 1
		fi

		qtBasePath="${QT_BASE_PATH}/${qtVersion}"
		qtArch="gcc_64"
	else
		echo "Using cmake's auto-detection of Qt headers and libraries"
		echo "QT_BASE_PATH env variable can be defined to the root folder of Qt installation (where MaintenanceTool resides)"
	fi
else
	echo "Unsupported platform"
	exit 1
fi

if [ -n "${qtBasePath}" ];
then
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
	add_cmake_opt+=("-DQt5_DIR=${qtBasePath}/${qtArch}/lib/cmake/Qt5")
fi

echo "Generating cmake project..."
"$cmake_path" -H. -B"${outputFolder}" "-G${generator}" $generator_arch_option $toolset_option $sdk_option $cmake_opt "${add_cmake_opt[@]}" $cmake_config

echo ""
echo "All done, generated project lies in ${outputFolder}"
