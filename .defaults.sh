# Default values for gen_cmake and gen_install

default_qt_version="5.15.2"
default_win_basePath="C:/Qt"
default_win_arch="msvc2019"
default_mac_basePath="/Applications/Qt"
default_mac_arch="clang_64"
default_linux_basePath="/usr/lib"
default_linux_arch="$(g++ -dumpmachine)"

# gen_cmake defaults
function extend_gc_fnc_defaults()
{
  default_VisualGenerator="Visual Studio 17 2022"
  default_VisualToolset="v143"
  default_VisualToolchain="x64"
  default_VisualArch="x86"
  default_keyDigits=2
  default_betaTagName="-beta"
}

# gen_install defaults
function extend_gi_fnc_defaults()
{
  default_VisualGenerator="Visual Studio 17 2022"
  default_VisualToolset="v143"
  default_VisualToolchain="x64"
  default_VisualArch="x86"
  default_keyDigits=2
  default_betaTagName="-beta"
}

# Some helper functions
function build_qt_config_folder()
{
	local -n _retval="$1"
	local basePath="$2"
	local arch="$3"
	local majorVers="$4"
	local result=""

	if isWindows; then
		result="${basePath}/${arch}/lib/cmake"
	elif isMac; then
		result="${basePath}/${arch}/lib/cmake"
	elif isLinux; then
		which g++ &> /dev/null
		if [ $? -ne 0 ];
		then
			echo "ERROR: g++ not found"
			exit 4
		fi
		result="${basePath}/${arch}/cmake"
	fi

	_retval="${result}"
}

function get_default_qt_path()
{
	local -n _retval="$1"
	local gdqp_result="" # Use a unique name for the result as we pass it by reference

	if isWindows; then
		build_qt_config_folder gdqp_result "${default_win_basePath}/${default_qt_version}" "${default_win_arch}" "${QtMajorVersion}"
	elif isMac; then
		build_qt_config_folder gdqp_result "${default_mac_basePath}/${default_qt_version}" "${default_mac_arch}" "${QtMajorVersion}"
	elif isLinux; then
		build_qt_config_folder gdqp_result "${default_linux_basePath}" "${default_linux_arch}" "${QtMajorVersion}"
	fi

	_retval="${gdqp_result}"
}
