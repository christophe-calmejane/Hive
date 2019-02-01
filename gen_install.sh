#!/bin/bash
# Useful script to generate installer

# Get absolute folder for this script
callerFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/" # Command to get the absolute path

# Include util functions
. "${callerFolderPath}3rdparty/avdecc/scripts/utils.sh"

# Override default cmake options
cmake_opt="-DENABLE_HIVE_CPACK=TRUE -DENABLE_HIVE_SIGNING=FALSE"

############################ DO NOT MODIFY AFTER THAT LINE #############

# Default values
default_VisualGenerator="Visual Studio 15 2017"
default_VisualToolset="v141"
default_VisualToolchain="x64"
default_VisualArch="x86"
default_VisualSdk="8.1"

# 
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

outputFolder="./_install_${arch}"
installSubFolder="/Install"
buildConfig="Release"
buildConfigOverride=0
doCleanup=1
doSign=1
gen_cmake_additional_options=()

# First check for .identity file
if isMac;
then
	if [ -f ".identity" ];
	then
		identityString="$(< .identity)"
		# Quick check for identity in keychain
		security find-identity -v -p codesigning | grep "$identityString" &> /dev/null
		if [ $? -ne 0 ];
		then
			echo "Invalid .identity file content (identity not found in keychain, or not valid for codesigning): $identityString"
			exit 1
		fi
		gen_cmake_additional_options+=("-id")
		gen_cmake_additional_options+=("$identityString")
		hasTeamId=1
	fi
fi

while [ $# -gt 0 ]
do
	case "$1" in
		-h)
			echo "Usage: gen_install.sh [options]"
			echo " -h -> Display this help"
			echo " -b <cmake path> -> Force cmake binary path (Default: $cmake_path)"
			echo " -c <cmake generator> -> Force cmake generator (Default: $generator)"
			echo " -noclean -> Don't remove temp build folder [Default=clean on successful build]"
			if isWindows; then
				echo " -t <visual toolset> -> Force visual toolset (Default: $toolset)"
				echo " -tc <visual toolchain> -> Force visual toolchain (Default: $toolchain)"
				echo " -64 -> Generate the 64 bits version of the project (Default: 32)"
			fi
			if isMac; then
				echo " -id <TeamIdentifier> -> iTunes team identifier for binary signing (or content of .identity file)."
			fi
			echo " -no-signing -> Do not sign binaries (Default: Do signing)"
			echo " -debug -> Compile using Debug configuration (Default: Release)"
			exit 3
			;;
		-noclean)
			doCleanup=0
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
			gen_cmake_additional_options+=("-c")
			gen_cmake_additional_options+=("$1")
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
				arch="x64"
				gen_cmake_additional_options+=("-64")
			else
				echo "ERROR: -64 option is only supported on Windows platform"
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
				gen_cmake_additional_options+=("-id")
				gen_cmake_additional_options+=("$1")
				identityString="$1"
				hasTeamId=1
			else
				echo "ERROR: -id option is only supported on macOS platform"
				exit 4
			fi
			;;
		-no-signing)
			doSign=0
			;;
		-debug)
			buildConfig="Debug"
			buildConfigOverride=1
			;;
		*)
			echo "ERROR: Unknown option '$1' (use -h for help)"
			exit 4
			;;
	esac
	shift
done

if [ $doSign -eq 1 ]; then
	gen_cmake_additional_options+=("-sign")

	# Check if TeamIdentifier is specified on macOS
	if isMac; then
		if [ $hasTeamId -eq 0 ]; then
			echo "ERROR: macOS requires either iTunes TeamIdentifier to be specified using -id option, or -no-signing to disable binary signing"
			exit 4
		fi
	fi
fi

toolset_option=""
if [ ! -z "${toolset}" ]; then
	toolset_option="-t ${toolset}"
	if [ ! -z "${toolchain}" ]; then
		toolset_option="${toolset_option} -tc ${toolchain}"
	fi
fi

# Cleanup routine
cleanup_main()
{
	if [[ $doCleanup -eq 1 && $1 -eq 0 ]]; then
		echo -n "Cleaning... "
		sleep 2
		rm -rf "${callerFolderPath}${outputFolder}"
		echo "done"
	else
		echo "Not cleaning up as requested, folder '${outputFolder}' untouched"
	fi
	exit $1
}

trap 'cleanup_main $?' EXIT

# Cleanup previous build folders, just in case
rm -rf "${callerFolderPath}${outputFolder}"

cmakeHiveVersion=$(grep "HIVE_VERSION" CMakeLists.txt | perl -nle 'print $& if m{VERSION[ ]+\K[^ )]+}')
if [[ $cmakeHiveVersion == "" ]]; then
	echo "Cannot detect project version"
	exit 1
fi

# Check if we have a release or devel version
oldIFS="$IFS"
IFS='.' read -a versionSplit <<< "$cmakeHiveVersion"
IFS="$oldIFS"

if [[ ${#versionSplit[*]} -lt 3 || ${#versionSplit[*]} -gt 4 ]]; then
	echo "Invalid project version (should be in the form x.y.z[.w])"
	exit 1
fi

beta_tag=""
build_tag=""
is_release=1
releaseVersion="${versionSplit[0]}.${versionSplit[1]}.${versionSplit[2]}"
if [ ${#versionSplit[*]} -eq 4 ]; then
	beta_tag="-beta${versionSplit[3]}"
	build_tag="+$(git rev-parse --short HEAD)"
	is_release=0
fi

if isWindows; then
	installerOSName="win32"
	latestVersionOSName="windows"
	installerExtension="exe"
elif isMac; then
	installerOSName="Darwin"
	latestVersionOSName="macOS"
	installerExtension="dmg"
else
	getOS osName
	echo "ERROR: Installer for $osName not supported yet"
	exit 1
fi

installerBaseName="Hive-${releaseVersion}${beta_tag}${build_tag}+${installerOSName}"
if [ $buildConfigOverride -eq 1 ]; then
	installerBaseName="${installerBaseName}+${buildConfig}"
fi
fullInstallerName="${installerBaseName}.${installerExtension}"

if [ -f *"${fullInstallerName}" ]; then
	echo "Installer already exists for version ${version}, please remove it first."
	exit 1
fi

echo -n "Generating cmake files... "
log=$(./gen_cmake.sh -o "${outputFolder}" -a "-DHIVE_INSTALLER_NAME=${installerBaseName}" "${gen_cmake_additional_options[@]}" $toolset_option -f "$cmake_opt")
if [ $? -ne 0 ]; then
	echo "Failed to generate cmake files ;("
	echo ""
	echo $log
	exit 1
fi
echo "done"

pushd "${outputFolder}" &> /dev/null
echo -n "Building project... "
log=$("$cmake_path" --build . --clean-first --config "${buildConfig}" --target Hive)
if [ $? -ne 0 ]; then
	echo "Failed:"
	echo ""
	echo $log
	exit 1
fi
# For some reason, for macOS signing to work properly, we need to run deployqt twice, so let's run it now, it will be run again during "package" target
log=$("$cmake_path" --build . --config "${buildConfig}" --target Hive_deployqt)
if [ $? -ne 0 ]; then
	echo "Failed:"
	echo ""
	echo $log
	exit 1
fi
echo "done"
popd &> /dev/null

pushd "${outputFolder}" &> /dev/null
echo -n "Generating project installer... "
log=$("$cmake_path" --build . --config "${buildConfig}" --target package)
if [ $? -ne 0 ]; then
	echo "Failed:"
	echo ""
	echo $log
	exit 1
fi
echo "done"
popd &> /dev/null

which tar &> /dev/null
if [ $? -eq 0 ]; then
	symbolsFile="${installerBaseName}-symbols.tgz"
	echo -n "Archiving symbols... "
	log=$(tar cvzf "${symbolsFile}" ${outputFolder}/Symbols)
	if [ $? -ne 0 ]; then
		echo "Failed to archive symbols ;("
		echo ""
		echo $log
		exit 1
	fi
	echo "done"
fi

#pushd "${outputFolder}" &> /dev/null
#echo -n "Signing package... "
#log=$("$cmake_path" --build . --config "${buildConfig}" --target SignPackage)
#if [ $? -ne 0 ]; then
#	echo "Failed to sign package ;("
#	echo ""
#	echo $log
#	exit 1
#fi
#echo "done"
#popd &> /dev/null

installerFile=$(ls ${outputFolder}/*${fullInstallerName})
if [ ! -f "$installerFile" ]; then
	echo "ERROR: Cannot find installer file in $outputFolder sub folder. Not cleaning it so you can manually search."
	doCleanup=0
	exit 1
fi

if isMac && [[ hasTeamId -eq 1 && doSign -eq 1 ]];
then
	echo "Signing Package"
	codesign -s "${identityString}" --timestamp --verbose=4 --strict --force "${installerFile}"
fi

mv "${installerFile}" .

echo ""
echo "Installer generated: ${fullInstallerName}"
if [ ! -z "${symbolsFile}" ]; then
	echo "Symbols generated: ${symbolsFile}"
fi

if [ $is_release -eq 1 ]; then
	echo "${cmakeHiveVersion}" > "LatestVersion-${latestVersionOSName}.txt"
else
	echo "${cmakeHiveVersion}" > "LatestVersion-beta-${latestVersionOSName}.txt"
fi

exit 0
