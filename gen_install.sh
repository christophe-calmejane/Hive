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
doSign=0
gen_cmake_additional_options=()

while [ $# -gt 0 ]
do
	case "$1" in
		-h)
			echo "Usage: gen_install.sh [options]"
			echo " -h -> Display this help"
			echo " -b <cmake path> -> Force cmake binary path (Default: $cmake_path)"
			echo " -c <cmake generator> -> Force cmake generator (Default: $generator)"
			echo " -noclean -> Don't remove temp build folder [Default=clean]"
			if isWindows; then
				echo " -t <visual toolset> -> Force visual toolset (Default: $toolset)"
				echo " -tc <visual toolchain> -> Force visual toolchain (Default: $toolchain)"
				echo " -64 -> Generate the 64 bits version of the project (Default: 32)"
			fi
			if isMac; then
				echo " -id <TeamIdentifier> -> iTunes team identifier for binary signing."
			fi
			echo " -sign -> Sign binaries (Default: No signing)"
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
				add_cmake_opt="$add_cmake_opt -DLA_TEAM_IDENTIFIER=$1"
				hasTeamId=1
			else
				echo "ERROR: -id option is only supported on macOS platform"
				exit 4
			fi
			;;
		-sign)
			gen_cmake_additional_options+=("-sign")
			doSign=1
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

# Check TeamIdentifier specified is signing enabled on macOS
if isMac; then
	if [[ $hasTeamId -eq 0 && $doSign -eq 1 ]]; then
		echo "ERROR: macOS requires either iTunes TeamIdentifier to be specified using -id option, or -no-signing to disable binary signing"
		exit 4
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
	if [ $doCleanup -eq 1 ]; then
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

hiveVersion=$(grep "HIVE_VERSION" CMakeLists.txt | perl -nle 'print $& if m{VERSION[ ]+\K[^ )]+}')
if [[ $hiveVersion == "" ]]; then
	echo "Cannot detect project version"
	exit 1
fi

# Check if we have a release or devel version
oldIFS="$IFS"
IFS='.' read -a versionSplit <<< "$hiveVersion"
IFS="$oldIFS"

build_tag=""
if [ ${#versionSplit[*]} -eq 4 ]; then
	add_cmake_opt="$add_cmake_opt -DAVDECC_BASE_FOLDER=3rdparty/avdecc-local"
	build_tag="-$(git rev-parse --short HEAD)"
fi

if isWindows; then
	installerOSName="win32"
	installerExtension="exe"
elif isMac; then
	installerOSName="Darwin"
	installerExtension="dmg"
else
	getOS osName
	echo "ERROR: Installer for $osName not supported yet"
	exit 1
fi

installerBaseName="Hive-${hiveVersion}${build_tag}-${installerOSName}"
if [ $buildConfigOverride -eq 1 ]; then
	installerBaseName="${installerBaseName}-${buildConfig}"
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
log=$("$cmake_path" --build . --clean-first --config "${buildConfig}" --target install)
if [ $? -ne 0 ]; then
	echo "Failed to build project ;("
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
	echo "Failed to generate installer ;("
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
mv "${installerFile}" .

echo ""
echo "Installer generated: ${fullInstallerName}"
if [ ! -z "${symbolsFile}" ]; then
	echo "Symbols generated: ${symbolsFile}"
fi

exit 0
