# Default values for gen_cmake and gen_install

# Qt defaults
default_qt_version="6.5.2"
default_qt_win_basePath="C:/Qt"
default_qt_win_arch="msvc2019"
default_qt_mac_basePath="/Applications/Qt"
default_qt_mac_arch="clang_64"
default_qt_linux_basePath="/usr/lib"
default_qt_linux_arch="$(g++ -dumpmachine)"

# gen_cmake defaults
function extend_gc_fnc_defaults()
{
	default_VisualGenerator="Visual Studio 17 2022"
	default_VisualToolset="v143"
	default_VisualToolchain="x64"
	default_VisualArch="x64"
  default_keyDigits=2
  default_betaTagName="-beta"
}

# gen_install defaults
function extend_gi_fnc_defaults()
{
  default_VisualGenerator="Visual Studio 17 2022"
  default_VisualToolset="v143"
  default_VisualToolchain="x64"
  default_VisualArch="x64"
  default_keyDigits=2
  default_betaTagName="-beta"
}

# Some helper functions
function get_default_qt_path()
{
	local -n _retval="$1"
	local result=""

	if isWindows; then
		result="${default_qt_win_basePath}"
	elif isMac; then
		result="${default_qt_mac_basePath}"
	elif isLinux; then
		result="${default_qt_linux_basePath}"
	fi

	_retval="${result}"
}

function get_default_qt_arch()
{
	local -n _retval="$1"
	local result=""

	if isWindows; then
		result="${default_qt_win_arch}"
	elif isMac; then
		result="${default_qt_mac_arch}"
	elif isLinux; then
		result="${default_qt_linux_arch}"
	fi

	_retval="${result}"
}
