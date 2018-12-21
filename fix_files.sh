#!/bin/bash

FIX_FILES_VERSION="1.2"

echo "Fix-Files version $FIX_FILES_VERSION"
echo ""

# Check bash version
if [[ ${BASH_VERSINFO[0]} < 5 && (${BASH_VERSINFO[0]} < 4 || ${BASH_VERSINFO[1]} < 1) ]]; then
  echo "bash 4.1 or later required"
  exit 255
fi

# currentFolderAbsolute="$(cd "$2"; pwd -P)/"

# Check if a .git file or folder exists (we allow submodules, thus check file and folder), as well as a root cmake file
if [[ ! -e ".git" || ! -f "CMakeLists.txt" ]]; then
	echo "ERROR: Must be run from the root folder of your project (where your main CMakeLists.txt file is)"
	exit 1
fi

do_clang_format=1
do_line_endings=1
do_chmod=1

while [ $# -gt 0 ]
do
	case "$1" in
		-h)
			echo "Usage: fix_files.sh [options]"
			echo " -h -> Display this help"
			echo " --no-clang-format -> Do not run clang-format on source files (Default: Run clang-format, but only if .clang-format file found)"
			echo " --no-line-endings -> Do not force line endings on source files (Default: Change line-endings)"
			echo " --no-chmod -> Do not run chmod on all files to fix executable bit (Default: Run chmod)"
			exit 3
			;;
		--no-clang-format)
			do_clang_format=0
			;;
		--no-line-endings)
			do_line_endings=0
			;;
		--no-chmod)
			do_chmod=0
			;;
		*)
			echo "ERROR: Unknown option '$1' (use -h for help)"
			exit 4
			;;
	esac
	shift
done


function applyFormat()
{
	local filePattern="$1"
	
	echo "Formatting all $filePattern files"
	find . -iname "$filePattern" -not -path "./3rdparty/*" -not -path "./externals/*" -not -path "./_*" -exec clang-format -i -style=file {} \;
}

function applyFileAttributes()
{
	local filePattern="$1"
	local attribs="$2"
	
	echo "Setting file attributes for all $filePattern files"
	find . -iname "$filePattern" -not -path "./3rdparty/*" -not -path "./externals/*" -not -path "./_*" -exec chmod "$attribs" {} \;
}

function applyLineEndings()
{
	local filePattern="$1"
	
	find . -iname "$filePattern" -not -path "./3rdparty/*" -not -path "./externals/*" -not -path "./_*" -exec dos2unix {} \;
}

if [[ $do_clang_format -eq 1 && -f ./.clang-format ]]; then
	which clang-format &> /dev/null
	if [ $? -eq 0 ]; then
		cf_version="$(clang-format --version)"
		if [ "$cf_version" != "clang-format version 7.0.0 (tags/RELEASE_700/final/WithWrappingBeforeLambdaBodyPatch)" ]; then
			echo "Incorrect clang-format: Version 7.0.0 with WrappingBeforeLambdaBody patch required"
			exit 1
		fi
		applyFormat "*.[chi]pp"
		applyFormat "*.[ch]"
		applyFormat "*.mm"
	else
		echo "clang-format required"
		exit 1
	fi
fi

if [ $do_chmod -eq 1 ]; then
	which dos2unix &> /dev/null
	if [ $? -eq 0 ]; then
		applyLineEndings "*.[chi]pp"
		applyLineEndings "*.[ch]"
		applyLineEndings "*.mm"
		applyLineEndings "*.js"
		applyLineEndings "*.txt"
		applyLineEndings "*.in"
		applyLineEndings "*.cmake"
		applyLineEndings "*.md"
		applyLineEndings "*.sh"
	else
		echo "dos2unix command not found, not changing file line endings"
	fi
fi

if [ $do_line_endings -eq 1 ]; then
	which chmod &> /dev/null
	if [ $? -eq 0 ]; then
		# Text/source files (non-executable)
		chmod a-x .gitignore .gitmodules COPYING COPYING.LESSER
		applyFileAttributes "*.[chi]pp" "a-x"
		applyFileAttributes "*.[ch]" "a-x"
		applyFileAttributes "*.mm" "a-x"
		applyFileAttributes "*.js" "a-x"
		applyFileAttributes "*.txt" "a-x"
		applyFileAttributes "*.in" "a-x"
		applyFileAttributes "*.cmake" "a-x"
		applyFileAttributes "*.md" "a-x"
		applyFileAttributes "*.patch" "a-x"
		applyFileAttributes "*.ui" "a-x"

		# Other files (non-executable)
		applyFileAttributes "*.svg" "a-x"
		applyFileAttributes "*.png" "a-x"
		applyFileAttributes "*.qrc" "a-x"
		
		# Binary files (executable)
		applyFileAttributes "*.sh" "a+x"
		applyFileAttributes "*.bat" "a+x"
		applyFileAttributes "*.exe" "a+x"
		applyFileAttributes "*.dll" "a+x"
	else
		echo "chmod command not found, not changing file attributes"
	fi
fi
