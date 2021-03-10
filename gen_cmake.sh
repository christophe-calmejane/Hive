#!/usr/bin/env bash
# Useful script to generate project files using cmake

# Qt Version to use
qtVersion="5.15.2"

# Override default cmake options
cmake_opt="-DENABLE_HIVE_CPACK=FALSE -DENABLE_HIVE_SIGNING=FALSE"

############################ DO NOT MODIFY AFTER THAT LINE #############

# Get absolute folder for this script
selfFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/" # Command to get the absolute path

# Include util functions
. "${selfFolderPath}3rdparty/avdecc/scripts/bashUtils/utils.sh"

# Check if setup_fresh_env has been called
if [ ! -f "${selfFolderPath}resources/dsa_pub.pem" ]; then
	echo "ERROR: Please run setup_fresh_env.sh (just once) after having cloned this repository."
	exit 4
fi

# Default values
default_VisualGenerator="Visual Studio 16 2019"
default_VisualGeneratorArch="Win32"
default_VisualToolset="v142"
default_VisualToolchain="x64"
default_VisualArch="x86"
default_signtoolOptions="/a /sm /q /fd sha256 /tr http://timestamp.sectigo.com"

# 
cmake_generator=""
generator_arch=""
platform=""
default_arch=""
arch=""
toolset=""
cmake_config=""
outputFolderBasePath="_build"
defaultOutputFolder="${outputFolderBasePath}_<platform>_<arch>_<generator>_<toolset>_<config>"
declare -a supportedArchs=()
if isMac; then
	cmake_path="/Applications/CMake.app/Contents/bin/cmake"
	# CMake.app not found, use cmake from the path
	if [ ! -f "${cmake_path}" ]; then
		cmake_path="cmake"
	fi
	generator="Xcode"
	getMachineArch default_arch
	supportedArchs+=("${default_arch}")
else
	# Use cmake from the path
	cmake_path="cmake"
	if isWindows; then
		generator="$default_VisualGenerator"
		generator_arch="$default_VisualGeneratorArch"
		toolset="$default_VisualToolset"
		toolchain="$default_VisualToolchain"
		default_arch="$default_VisualArch"
		supportedArchs+=("x86")
		supportedArchs+=("x64")
	else
		generator="Unix Makefiles"
		getMachineArch default_arch
		supportedArchs+=("${default_arch}")
	fi
fi
getOS platform

which "${cmake_path}" &> /dev/null
if [ $? -ne 0 ]; then
	echo "CMake not found. Please add CMake binary folder in your PATH environment variable."
	exit 1
fi

outputFolder=""
outputFolderForced=0
add_cmake_opt=()
useVSclang=0
useVS2017=0
signingId=""
doSign=0
signtoolOptions="$default_signtoolOptions"
useSources=0
overrideQt5dir=0
Qt5dir=""

while [ $# -gt 0 ]
do
	case "$1" in
		-h)
			echo "Usage: gen_cmake.sh [options]"
			echo " -h -> Display this help"
			echo " -o <folder> -> Output folder (Default: ${defaultOutputFolder})"
			echo " -f <flags> -> Force all cmake flags (Default: $cmake_opt)"
			echo " -a <flags> -> Add cmake flags to default ones (or to forced ones with -f option)"
			echo " -b <cmake path> -> Force cmake binary path (Default: $cmake_path)"
			echo " -c <cmake generator> -> Force cmake generator (Default: $generator)"
			echo " -arch <arch> -> Set target architecture (Default: $default_arch). Supported archs depends on target platform"
			if isWindows; then
				echo " -t <visual toolset> -> Force visual toolset (Default: $toolset)"
				echo " -tc <visual toolchain> -> Force visual toolchain (Default: $toolchain)"
				echo " -64 -> Generate the 64 bits version of the project (Default: 32)"
				echo " -vs2017 -> Compile using VS 2017 compiler instead of the default one"
				echo " -clang -> Compile using clang for VisualStudio"
				echo " -signtool-opt <options> -> Windows code signing options (Default: $default_signtoolOptions)"
			fi
			if isMac; then
				echo " -id <Signing Identity> -> Signing identity for binary signing (full identity name inbetween the quotes, see -ids to get the list)"
				echo " -ids -> List signing identities"
				echo " -t <xcode toolset> -> Force xcode toolset (Default: autodetect)"
				echo " -ios -> Cross-compiling for iOS"
			fi
			echo " -android -> Cross-compiling for Android"
			echo " -debug -> Force debug configuration (Single-Configuration generators only)"
			echo " -release -> Force release configuration (Single-Configuration generators only)"
			echo " -sign -> Sign binaries (Default: No signing)"
			echo " -source -> Use Qt source instead of precompiled binarie (Default: Not using sources). Do not use with -qt5dir option."
			echo " -qt5dir <Qt5 CMake Folder> -> Override automatic Qt5_DIR detection with the full path to the folder containing Qt5Config.cmake file (either binary or source)"
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
			IFS=' ' read -r -a tokens <<< "$1"
			for token in ${tokens[@]}
			do
				add_cmake_opt+=("$token")
			done
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
			if [[ $(isWindows; echo $?) -eq 0 || $(isMac; echo $?) -eq 0 ]]; then
				shift
				if [ $# -lt 1 ]; then
					echo "ERROR: Missing parameter for -t option, see help (-h)"
					exit 4
				fi
				toolset="$1"
			else
				echo "ERROR: -t option is only supported on Windows/macOS platforms"
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
				generator="Visual Studio 15 2017 Win64"
				arch="x64" # Changing arch not default_arch, as this is not a cross-compilation option
			else
				echo "ERROR: -64 option is only supported on Windows platform"
				exit 4
			fi
			;;
		-vs2017)
			if isWindows; then
				useVS2017=1
			else
				echo "ERROR: -vs2017 option is only supported on Windows platform"
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
		-signtool-opt)
			if isWindows; then
				shift
				if [ $# -lt 1 ]; then
					echo "ERROR: Missing parameter for -signtool-opt option, see help (-h)"
					exit 4
				fi
				signtoolOptions="$1"
			else
				echo "ERROR: -signtool-opt option is only supported on Windows platform"
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
				signingId="$1"
			else
				echo "ERROR: -id option is only supported on macOS platform"
				exit 4
			fi
			;;
		-ids)
			if isMac; then
				security find-identity -v -p codesigning | grep -Po "^[[:space:]]+[0-9]+\)[[:space:]]+[0-9A-Z]+[[:space:]]+\"\KDeveloper ID Application: [^(]+\([^)]+\)(?=\")"
				if [ $? -ne 0 ]; then
					echo "ERROR: No valid signing identity found. You must install a 'Developer ID Application' certificate"
					exit 4
				fi
				exit 0
			else
				echo "ERROR: -ids option is only supported on macOS platform"
				exit 4
			fi
			;;
		-ios)
			if isMac; then
				platform="ios"
				default_arch="arm64" # Setting default arch for cross-compilation
				supportedArchs+=("arm")
				supportedArchs+=("arm64")
				add_cmake_opt+=("-DCMAKE_SYSTEM_NAME=iOS")
			else
				echo "ERROR: -ios option is only supported on MacOS platform"
				exit 4
			fi
			;;
		-android)
			if [ "x${ANDROID_NDK_HOME}" == "x" ]; then
				echo "ERROR: ANDROID_NDK_HOME env var required for Android cross-compilation"
				exit 4
			fi
			if [ ! -d "${ANDROID_NDK_HOME}" ]; then
				echo "ERROR: ANDROID_NDK_HOME env var does not point to a valid folder: ${ANDROID_NDK_HOME}"
				exit 4
			fi
			if [ ! -f "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" ]; then
				echo "ERROR: ANDROID_NDK_HOME env var does not point to a valid NDK folder (build/cmake/android.toolchain.cmake not found): ${ANDROID_NDK_HOME}"
				exit 4
			fi
			generator="Ninja"
			toolset=""
			toolchain="clang"
			platform="android"
			default_arch="arm64" # Setting default arch for cross-compilation
			supportedArchs+=("arm")
			supportedArchs+=("arm64")
			add_cmake_opt+=("-DCMAKE_SYSTEM_NAME=Android")
			add_cmake_opt+=("-DANDROID_TOOLCHAIN=${toolchain}")
			add_cmake_opt+=("-DCMAKE_ANDROID_NDK=$ANDROID_NDK_HOME")
			add_cmake_opt+=("-DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake")
			add_cmake_opt+=("-DCMAKE_ANDROID_STL_TYPE=gnustl_static")
			add_cmake_opt+=("-DANDROID_NATIVE_API_LEVEL=24")
			;;
		-arch)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -arch option, see help (-h)"
				exit 4
			fi
			arch="$1"
			;;
		-debug)
			cmake_config="Debug"
			;;
		-release)
			cmake_config="Release"
			;;
		-sign)
			doSign=1
			;;
		-source)
			useSources=1
			;;
		-qt5dir)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -qt5dir option, see help (-h)"
				exit 4
			fi
			overrideQt5dir=1
			Qt5dir="$1"
			;;
		*)
			echo "ERROR: Unknown option '$1' (use -h for help)"
			exit 4
			;;
	esac
	shift
done

if [[ $useSources -eq 1 && $overrideQt5dir -eq 1 ]]; then
	echo "ERROR: Cannot use -source and -qt5dir options at the same time."
	echo "If you want to use a custom source folder, just set -qt5dir to <Qt Source Root Folder>/qtbase/lib/cmake/Qt5"
	exit 4
fi

if [ ! -z "$cmake_generator" ]; then
	echo "Overriding default cmake generator ($generator) with: $cmake_generator"
	generator="$cmake_generator"
fi

# Default arch has not been overridden, use default arch
if [ "x${arch}" == "x" ]; then
	arch="${default_arch}"
fi

# Check arch is valid for target platform
if [[ ! " ${supportedArchs[@]} " =~ " ${arch} " ]]; then
	echo "ERROR: Unsupported arch for target platform: ${arch} (Supported archs: ${supportedArchs[@]})"
	exit 4
fi

# Special case for Android cross-compilation, we must set the correct ABI
if [ "${platform}" == "android" ];
then
	case "${arch}" in
		x86)
			add_cmake_opt+=("-DANDROID_ABI=x86")
			;;
		x64)
			add_cmake_opt+=("-DANDROID_ABI=x86_64")
			;;
		arm64)
			add_cmake_opt+=("-DANDROID_ABI=arm64-v8a")
			;;
		*)
			echo "ERROR: Unknown android arch: ${arch} (add support for it)"
			exit 4
			;;
	esac
fi

# Signing is now mandatory for macOS
if isMac; then
	# No signing identity provided, try to autodetect
	if [ "$signingId" == "" ]; then
		echo -n "No code signing identity provided, autodetecting... "
		signingId="$(security find-identity -v -p codesigning | grep -Po "^[[:space:]]+[0-9]+\)[[:space:]]+[0-9A-Z]+[[:space:]]+\"\KDeveloper ID Application: [^(]+\([^)]+\)(?=\")" -m1)"
		if [ "$signingId" == "" ]; then
			echo "ERROR: Cannot autodetect a valid code signing identity, please use -id option"
			exit 4
		fi
		echo "using identity: '$signingId'"
	fi
	# Validate code signing identity exists and is of 'Developer ID Application' type
	subSign="${signingId//(/\\(}" # Need to escape ( and )
	subSign="${subSign//)/\\)}"
	security find-identity -v -p codesigning | grep -Po "^[[:space:]]+[0-9]+\)[[:space:]]+[0-9A-Z]+[[:space:]]+\"${subSign}" | grep "Developer ID Application" &> /dev/null
	if [ $? -ne 0 ]; then
		echo "ERROR: Code signing identity '${signingId}' not found, use the full identity name inbetween the quotes (see -ids to get a list of valid identities)"
		exit 4
	fi
	# Validate installer signing identity exists and is of 'Developer ID Installer' type
	signingInstallerId="${signingId/Developer ID Application/Developer ID Installer}"
	subSign="${signingInstallerId//(/\\(}" # Need to escape ( and )
	subSign="${subSign//)/\\)}"
	security find-identity -v | grep -Po "^[[:space:]]+[0-9]+\)[[:space:]]+[0-9A-Z]+[[:space:]]+\"${subSign}" | grep "Developer ID Installer" &> /dev/null
	if [ $? -ne 0 ]; then
		echo "ERROR: Installer signing identity '${signingInstallerId}' not found. Download or create it from your Apple Developer account (should be of type 'Developer ID Installer')"
		exit 4
	fi
	# Get Team Identifier from signing identity (for xcode)
	teamRegEx="[^(]+\(([^)]+)"
	if ! [[ $signingId =~ $teamRegEx ]]; then
		echo "ERROR: Failed to find Team Identifier in signing identity: $signingId"
		exit 4
	fi
	teamId="${BASH_REMATCH[1]}"
	if [ $doSign -eq 0 ]; then
		echo "Binary signing is mandatory since macOS Catalina, forcing it using ID '$signingId' (TeamID '$teamId')"
		doSign=1
	fi
	add_cmake_opt+=("-DLA_BINARY_SIGNING_IDENTITY=$signingId")
	add_cmake_opt+=("-DLA_INSTALLER_SIGNING_IDENTITY=$signingInstallerId")
	add_cmake_opt+=("-DLA_TEAM_IDENTIFIER=$teamId")
fi

if [ $doSign -eq 1 ]; then
	add_cmake_opt+=("-DENABLE_HIVE_SIGNING=TRUE")
	# Set signtool options if signing enabled on windows
	if isWindows; then
		if [ ! -z "$signtoolOptions" ]; then
			add_cmake_opt+=("-DLA_SIGNTOOL_OPTIONS=$signtoolOptions")
		fi
	fi
fi

# Get DSA public key (macOS needs it in the plist)
if isMac; then
	if [ ! -f "resources/dsa_pub.pem" ]; then
		echo "ERROR: Sparkle requires a DSA pub/priv pair to be setup. Re-run setup_fresh_env.sh if you just upgraded the project."
		exit 4
	fi
	dsaPubKey="$(< resources/dsa_pub.pem)"
	add_cmake_opt+=("-DHIVE_DSA_PUB_KEY=${dsaPubKey}")
fi

# Using -vs2017 option
if [ $useVS2017 -eq 1 ]; then
	generator="Visual Studio 15 2017"
	toolset="v141"
fi

# Using -clang option (shortcut to auto-define the toolset)
if [ $useVSclang -eq 1 ]; then
	toolset="ClangCL"
fi

# Check if at least a -debug or -release option has been passed for Single-Configuration generators
if isSingleConfigurationGenerator "$generator"; then
	if [ -z $cmake_config ]; then
		echo "ERROR: Single-Configuration generator '$generator' requires either -debug or -release option to be specified"
		exit 4
	fi
	add_cmake_opt+=("-DCMAKE_BUILD_TYPE=${cmake_config}")
else
	# Clear any -debug or -release passed to a Multi-Configurations generator
	cmake_config=""
fi

if [ $outputFolderForced -eq 0 ]; then
	getOutputFolder outputFolder "${outputFolderBasePath}" "${platform}" "${arch}" "${toolset}" "${cmake_config}" "${generator}"
fi

if ! isSingleConfigurationGenerator "$generator"; then
	generator_arch_option=""
	if [ ! -z "${generator_arch}" ]; then
		generator_arch_option="-A${generator_arch} "
	fi

	toolset_option=""
	if [ ! -z "${toolset}" ]; then
		# On macOS, only valid for Xcode generator
		if [[ $(isMac; echo $?) -eq 0 && "${generator}" != "Xcode" ]]; then
			echo "The toolset option (-t) is only valid for Xcode generator on macOS"
			exit 4
		fi
		if [ ! -z "${toolchain}" ]; then
			toolset_option="-T${toolset},host=${toolchain} "
		else
			toolset_option="-T${toolset} "
		fi
	fi
fi

# Check for legacy 'avdecc-local' folder
if [ -d "3rdparty/avdecc-local" ]; then
	echo "Legacy '3rdparty/avdecc-local' no longer used, please remove this folder/link"
	exit 1
fi
add_cmake_opt+=("-DAVDECC_BASE_FOLDER=3rdparty/avdecc")

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

echo "/--------------------------\\"
echo "| Generating cmake project"
echo "| - GENERATOR: ${generator}"
echo "| - PLATFORM: ${platform}"
echo "| - ARCH: ${arch}"
if [ ! -z "${toolset}" ]; then
	echo "| - TOOLSET: ${toolset}"
fi
if [ ! -z "${toolchain}" ]; then
	echo "| - TOOLCHAIN: ${toolchain}"
fi
if [ ! -z "${cmake_config}" ]; then
	echo "| - BUILD TYPE: ${cmake_config}"
fi
echo "\\--------------------------/"
echo ""

"$cmake_path" -H. -B"${outputFolder}" "-G${generator}" $generator_arch_option $toolset_option $cmake_opt "${add_cmake_opt[@]}"

echo ""
echo "All done, generated project lies in ${outputFolder}"
