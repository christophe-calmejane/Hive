#!/usr/bin/env bash
# Useful script to generate installer

# Get absolute folder for this script
callerFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/" # Command to get the absolute path

# Include utils functions
. "${callerFolderPath}3rdparty/avdecc/scripts/bashUtils/utils.sh"

# Include config file functions
. "${callerFolderPath}scripts/loadConfigFile.sh"

# Override default cmake options
cmake_opt="-DENABLE_HIVE_CPACK=TRUE -DENABLE_HIVE_SIGNING=FALSE"

############################ DO NOT MODIFY AFTER THAT LINE #############

# Sanity checks
if [[ ${BASH_VERSINFO[0]} < 5 && (${BASH_VERSINFO[0]} < 4 || ${BASH_VERSINFO[1]} < 1) ]];
then
	echo "bash 4.1 or later required"
	if isMac;
	then
		echo "Try invoking the script with 'bash $0' instead of just '$0'"
	fi
	exit 127
fi
if isMac;
then
	which tar &> /dev/null
	if [ $? -ne 0 ];
	then
		echo "tar required. Install it via HomeBrew"
		exit 127
	fi
	which grep &> /dev/null
	if [ $? -ne 0 ];
	then
		echo "GNU grep required. Install it via HomeBrew"
		exit 127
	fi
	grep --version | grep BSD &> /dev/null
	if [ $? -eq 0 ];
	then
		echo "GNU grep required (not macOS native grep version). Install it via HomeBrew:"
		echo " - Install HomeBrew with the following command: /usr/bin/ruby -e \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)\""
		echo " - Install coreutils and grep with the following command: brew install coreutils grep"
		echo " - Export brew path with the following command: export PATH=\"\$(brew --prefix coreutils)/libexec/gnubin:\$(brew --prefix grep)/libexec/gnubin:/usr/local/bin:\$PATH\""
		echo " - Optionally set this path command in your .bashrc"
		exit 127
	fi
fi

# Deploy symbols found in current directory
deploySymbols()
{
	local projectName="$1"
	local version="$2"
	local result=""

	if [ ${doDeploySym} -eq 0 ];
	then
		return
	fi

	if isWindows;
	then
		echo -n "Deploying symbol files... "

		local symbolsServerPath="${params["symbols_windows_pdb_server_path"]}"
		if [ -z "${symbolsServerPath}" ];
		then
			echo "FAILED: 'symbols_windows_pdb_server_path' variable not set in ${configFile}"
			return
		fi
		if isCygwin; then
			symbolsServerPath=$(cygpath -a -u "${symbolsServerPath}")
		fi
		if [ ! -d "${symbolsServerPath}" ];
		then
			echo "FAILED: Server path does not exist: '${symbolsServerPath}'"
			return
		fi
		if isCygwin; then
			symbolsServerPath=$(cygpath -a -w "${symbolsServerPath}")
		fi

		local symstorePath="${params["symbols_symstore_path"]}"
		if [ -z "${symstorePath}" ];
		then
			echo "FAILED: 'symbols_symstore_path' variable not set in ${configFile}"
			return
		fi
		if isCygwin; then
			symstorePath=$(cygpath -a -u "${symstorePath}")
		fi
		if [ ! -f "${symstorePath}" ];
		then
			echo "FAILED: symstore.exe path does not exist: '${symstorePath}'"
			return
		fi

		result=$("${symstorePath}" add /r /l /f *.* /s "${symbolsServerPath}" /t "${projectName}" /v "${version}" /c "Adding ${projectName} ${version}" /compress)
		if [ $? -ne 0 ]; then
			echo "Failed to deploy symbols ;("
			echo ""
			echo $result
			return
		fi

		echo "done"

	elif isMac;
	then
		echo -n "Deploying symbol files... "

		local symbolsServerPath="${params["symbols_macos_dsym_server_path"]}"
		if [ -z "${symbolsServerPath}" ];
		then
			echo "FAILED: 'symbols_macos_dsym_server_path' variable not set in ${configFile}"
			return
		fi
		if [ ! -d "${symbolsServerPath}" ];
		then
			echo "FAILED: Server path does not exist: '${symbolsServerPath}'"
			return
		fi
		symbolsServerPath="${symbolsServerPath}/${projectName}/${version}"
		if [ ! -d "${symbolsServerPath}" ]; then
			mkdir -p "${symbolsServerPath}"
		fi
		for sym in `ls`
		do
			#rm -rf "${symbolsServerPath}/${sym}"
			cp -R "${sym}" "${symbolsServerPath}/"
		done

		echo "done"
	fi
}

getSignatureHash()
{
	local filePath="$1"
	local privKey="$2"
	local _retval="$3"
	local result=""

	if isWindows;
	then
		result=$(openssl dgst -sha1 -binary < "$filePath" | openssl dgst -sha1 -sign "$privKey" | openssl enc -base64)

	elif isMac;
	then
		local signUpdateFile="3rdparty/sparkle/sign_update"
		if [ ! -f "$signUpdateFile" ];
		then
			echo "ERROR: $signUpdateFile not found"
			exit 1
		fi
		result=$(3rdparty/sparkle/sign_update "$filePath" | cut -d '"' -f 2)

	else
		echo "getSignatureHash: TODO"
	fi

	eval $_retval="'${result}'"
}

generateAppcast()
{
	local fileName="$1"
	local buildNumber="$2"
	local marketingVersion="$3"
	local isRelease=$4

	local appcastFile="appcastItem-${marketingVersion}.xml"
	local baseURL="${params["appcast_releases"]}"
	local subPath="release"

	local fileSize
	getFileSize "$fileName" fileSize

	local fileSignature
	getSignatureHash "$fileName" "resources/dsa_priv.pem" fileSignature

	if [ "x$fileSignature" == "x" ];
	then
		echo "Failed to generate Appcast: Cannot sign file"
		exit 1
	fi

	if [ $is_release -eq 0 ];
	then
		subPath="beta"
		baseURL="${params["appcast_betas"]}"
	fi

	# Get URL of folder containing appcast file
	baseURL="${baseURL%/*}/"

	# Common Appcast Item header
	echo "		<item>" > "$appcastFile"
	echo "			<title>Version $marketingVersion</title>" >> "$appcastFile"
	echo "			<sparkle:releaseNotesLink>" >> "$appcastFile"
	echo "				${baseURL}changelog.php?lastKnownVersion=next" >> "$appcastFile"
	echo "			</sparkle:releaseNotesLink>" >> "$appcastFile"
	echo "			<pubDate>`date -R`</pubDate>" >> "$appcastFile"
	echo "			<enclosure url=\"${baseURL}${subPath}/${fileName}\"" >> "$appcastFile"

	# OS-dependant Item values
	if isWindows;
	then
		echo "				sparkle:dsaSignature=\"${fileSignature}\"" >> "$appcastFile"
		echo "				sparkle:installerArguments=\"/S /NOPCAP /LAUNCH\"" >> "$appcastFile"
		echo "				sparkle:os=\"windows\"" >> "$appcastFile"

	elif isMac;
	then
		echo "				sparkle:edSignature=\"${fileSignature}\"" >> "$appcastFile"
		echo "				sparkle:os=\"macos\"" >> "$appcastFile"

	else
		echo "Appcast generation not support on this OS"
		return;
	fi

	# Common Appcast Item footer
	echo "				sparkle:shortVersionString=\"${marketingVersion}\"" >> "$appcastFile"
	echo "				sparkle:version=\"${buildNumber}\"" >> "$appcastFile"
	echo "				length=\"${fileSize}\"" >> "$appcastFile"
	echo "				type=\"application/octet-stream\"" >> "$appcastFile"
	echo "			/>" >> "$appcastFile"
	echo "		</item>" >> "$appcastFile"

	# Done
	echo "Appcast item generated to file: $appcastFile (add it to tools/webserver/appcast-${subPath}.xml)"
}

# Default values
default_VisualGenerator="Visual Studio 16 2019"
default_VisualGeneratorArch="Win32"
default_VisualToolset="v142"
default_VisualToolchain="x64"
default_VisualArch="x86"

# 
cmake_generator=""
platform=""
default_arch=""
arch=""
toolset=""
outputFolderBasePath="_install"
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
	supportedArchs+=("x64")
	supportedArchs+=("arm64")
else
	# Use cmake from the path
	cmake_path="cmake"
	if isWindows; then
		generator="$default_VisualGenerator"
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
buildConfig="Release"
buildConfigOverride=0
doCleanup=1
doSign=1
doDeploySym=1
gen_cmake_additional_options=()
if [ -z $default_keyDigits ]; then
	default_keyDigits=2
fi
key_digits=$((10#$default_keyDigits))
key_postfix=""

while [ $# -gt 0 ]
do
	case "$1" in
		-h)
			echo "Usage: gen_install.sh [options]"
			echo " -h -> Display this help"
			echo " -b <cmake path> -> Force cmake binary path (Default: $cmake_path)"
			echo " -c <cmake generator> -> Force cmake generator (Default: $generator)"
			echo " -arch <arch> -> Set target architecture (Default: $default_arch). Supported archs depends on target platform"
			echo " -noclean -> Don't remove temp build folder [Default=clean on successful build]"
			if isWindows; then
				echo " -t <visual toolset> -> Force visual toolset (Default: $toolset)"
				echo " -tc <visual toolchain> -> Force visual toolchain (Default: $toolchain)"
			fi
			echo " -no-sym -> Do not deploy debug symbols (Default: Do deploy)"
			echo " -no-signing -> Do not sign binaries (Default: Do signing)"
			echo " -debug -> Compile using Debug configuration (Default: Release)"
			echo " -key-digits <Number of digits> -> The number of digits to be used as Key for installation, comprised between 0 and 4 (Default: $default_keyDigits)"
			echo " -key-postfix <Postfix> -> Postfix string to be added to the Key for installation (Default: "")"
			echo " -qt5dir <Qt5 CMake Folder> -> Override automatic Qt5_DIR detection with the full path to the folder containing Qt5Config.cmake file (either binary or source)"
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
			cmake_generator="$1"
			;;
		-arch)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -arch option, see help (-h)"
				exit 4
			fi
			arch="$1"
			gen_cmake_additional_options+=("-arch")
			gen_cmake_additional_options+=("$arch")
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
			echo "ERROR: -64 option is deprecated, use -arch x64 instead"
			exit 4
			;;
		-id)
			echo "ERROR: -id option is deprecated, please use the new ${configFile} file (see ${configFile}.sample for an example config file)"
			exit 1
			;;
		-no-sym)
			doDeploySym=0
			;;
		-no-signing)
			doSign=0
			;;
		-debug)
			buildConfig="Debug"
			buildConfigOverride=1
			;;
		-key-digits)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -key-digits option, see help (-h)"
				exit 4
			fi
			numberRegex='^[0-9]$'
			if ! [[ $1 =~ $numberRegex ]]; then
				echo "ERROR: Invalid value for -key-digits option (not a number), see help (-h)"
				exit 4
			fi
			key_digits=$((10#$1))
			if [[ $key_digits -lt 0 || $key_digits -gt 4 ]]; then
				echo "ERROR: Invalid value for -key-digits option (not comprised between 0 and 4), see help (-h)"
				exit 4
			fi
			;;
		-key-postfix)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -key-postfix option, see help (-h)"
				exit 4
			fi
			postfixRegex='^[a-zA-Z0-9_+-]+$'
			if ! [[ $1 =~ $postfixRegex ]]; then
				echo "ERROR: Invalid value for -key-postfix option (Only alphanum, underscore, plus and minus are allowed), see help (-h)"
				exit 4
			fi
			key_postfix="$1"
			;;
		-qt5dir)
			shift
			if [ $# -lt 1 ]; then
				echo "ERROR: Missing parameter for -qt5dir option, see help (-h)"
				exit 4
			fi
			gen_cmake_additional_options+=("-qt5dir")
			gen_cmake_additional_options+=("$1")
			;;
		*)
			echo "ERROR: Unknown option '$1' (use -h for help)"
			exit 4
			;;
	esac
	shift
done

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

# Load config file
loadConfigFile

# Check for signing
if [ $doSign -eq 1 ]; then
	gen_cmake_additional_options+=("-sign")

	# Check if signtool options are specified on windows
	if isWindows; then
		signtoolOptions=${params["signtool_options"]}
		if [ -z "$signtoolOptions" ]; then
			echo "ERROR: windows requires signtool options to be set. Specify it in the ${configFile} file"
		fi
		gen_cmake_additional_options+=("-signtool-opt")
		gen_cmake_additional_options+=("$signtoolOptions")
	fi

	# Check if Identity is specified on macOS
	if isMac; then
		identityString="${params["identity"]}"

		if [ "x$identityString" == "x" ]; then
			echo "ERROR: macOS requires valid signing identity. Specify it in the ${configFile} file"
			exit 4
		fi
		gen_cmake_additional_options+=("-id")
		gen_cmake_additional_options+=("$identityString")
	fi
fi

# Additional options from .hive_config file
if [ "x${params["appcast_releases"]}" == "x" ]; then
	echo "ERROR: appcast_releases must not be empty in ${configFile} file"
	exit 4
fi
gen_cmake_additional_options+=("-a")
gen_cmake_additional_options+=("-DHIVE_APPCAST_RELEASES_URL=${params["appcast_releases"]}")

if [ "x${params["appcast_betas"]}" == "x" ]; then
	echo "ERROR: appcast_betas must not be empty in ${configFile} file"
	exit 4
fi
gen_cmake_additional_options+=("-a")
gen_cmake_additional_options+=("-DHIVE_APPCAST_BETAS_URL=${params["appcast_betas"]}")

if [ "x${params["appcast_releases_fallback"]}" != "x" ]; then
	gen_cmake_additional_options+=("-a")
	gen_cmake_additional_options+=("-DHIVE_APPCAST_RELEASES_FALLBACK_URL=${params["appcast_releases_fallback"]}")
fi

if [ "x${params["appcast_betas_fallback"]}" != "x" ]; then
	gen_cmake_additional_options+=("-a")
	gen_cmake_additional_options+=("-DHIVE_APPCAST_BETAS_FALLBACK_URL=${params["appcast_betas_fallback"]}")
fi

# Build marketing options
marketing_options="-DMARKETING_VERSION_DIGITS=${key_digits} -DMARKETING_VERSION_POSTFIX=${key_postfix}"

getOutputFolder outputFolder "${outputFolderBasePath}" "${platform}" "${arch}" "${toolset}" "${buildConfig}" "${generator}"

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

cmakeHiveVersion=$(grep -Po "set *\(.+_VERSION +\K[0-9]+(\.[0-9]+)+(?= *\))" CMakeLists.txt)
if [[ $cmakeHiveVersion == "" ]]; then
	echo "Cannot detect project version"
	exit 1
fi
cmakeHiveVersion="${cmakeHiveVersion//[$'\t\r\n']}"

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
internalVersion="$((${versionSplit[0]} * 1000000 + ${versionSplit[1]} * 1000 + ${versionSplit[2]}))"
if [ ${#versionSplit[*]} -eq 4 ]; then
	beta_tag="-beta${versionSplit[3]}"
	build_tag="+$(git rev-parse --short HEAD)"
	is_release=0
	internalVersion="${internalVersion}.${versionSplit[3]}"
else
	internalVersion="${internalVersion}.999"
fi

if isWindows; then
	installerOSName="win32"
	installerExtension="exe"
elif isMac; then
	installerOSName="Darwin"
	installerExtension="pkg"
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
	echo "Installer already exists for version ${releaseVersion}, please remove it first."
	exit 1
fi

echo -n "Generating cmake files... "
log=$(./gen_cmake.sh -o "${outputFolder}" -a "-DHIVE_INSTALLER_NAME=${installerBaseName} ${marketing_options}" "${gen_cmake_additional_options[@]}" $toolset_option -f "$cmake_opt")
if [ $? -ne 0 ]; then
	echo "Failed to generate cmake files ;("
	echo ""
	echo $log
	exit 1
fi
echo "done"

pushd "${outputFolder}" &> /dev/null
echo -n "Building project... "
log=$("$cmake_path" --build . -j 4 --clean-first --config "${buildConfig}" --target Hive)
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

installerFile=$(ls ${outputFolder}/*${fullInstallerName})
if [ ! -f "$installerFile" ]; then
	echo "ERROR: Cannot find installer file in $outputFolder sub folder. Not cleaning it so you can manually search."
	doCleanup=0
	exit 1
fi

if [ $doSign -eq 1 ]; then
	# MacOS already signed by CPack
	if isWindows; then
		echo -n "Signing Package..."
		log=$(signtool sign ${signtoolOptions} "${installerFile}")
		if [ $? -ne 0 ]; then
			echo "Failed to sign package ;("
			echo ""
			echo $log
			exit 1
		fi
		echo "done"
	fi
fi

mv "${installerFile}" .

echo ""
echo "Installer generated: ${fullInstallerName}"
if [ ! -z "${symbolsFile}" ]; then
	echo "Symbols generated: ${symbolsFile}"
	pushd "${outputFolder}/Symbols" &> /dev/null
	deploySymbols "Hive" "${internalVersion}"
	popd &> /dev/null
fi

appcastInstallerName="${fullInstallerName}"

# macOS specific code
if isMac; then
	# Call notarization
	if [ ! "x${params["notarization_username"]}" == "x" ]; then
		3rdparty/avdecc/scripts/bashUtils/notarize_binary.sh "${fullInstallerName}" "${params["notarization_username"]}" "${params["notarization_password"]}" "com.KikiSoft.Hive.${installerExtension}"
		if [ $? -ne 0 ]; then
			echo "Failed to notarize installer"
			exit 1
		fi
	fi
	# Tar the installer as Sparkle do not support PKG
	appcastInstallerName="${fullInstallerName}.tar"
	tar cvf "${appcastInstallerName}" "${fullInstallerName}" &> /dev/null
	rm -f "${fullInstallerName}" &> /dev/null
fi

generateAppcast "${appcastInstallerName}" "${internalVersion}" "${releaseVersion}${beta_tag}" $is_release

echo ""
echo "Do not forget to upload:"
echo " - CHANGELOG.MD"
echo " - Installer file: ${appcastInstallerName}"
echo " - Updated appcast file(s)"

exit 0
