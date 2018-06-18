#!/bin/bash

# Check bash version
if [[ ${BASH_VERSINFO[0]} < 5 && (${BASH_VERSINFO[0]} < 4 || ${BASH_VERSINFO[1]} < 1) ]]; then
  echo "bash 4.1 or later required"
  exit 255
fi

if [ ! -f "./fix_files.sh" ]; then
  echo "ERROR: Script must be run from its own folder"
  exit 1
fi

function applyFileAttributes()
{
	local filePattern="$1"
	local attribs="$2"
	
	find . -iname "$filePattern" -not -path "./3rdparty/*" -not -path "./_*" -exec chmod "$attribs" {} \;
}

function applyLineEndings()
{
	local filePattern="$1"
	
	find . -iname "$filePattern" -not -path "./3rdparty/*" -not -path "./_*" -exec dos2unix {} \;
}

which dos2unix &> /dev/null
if [ $? -eq 0 ]; then
	applyLineEndings "*.[ch]pp"
	applyLineEndings "*.[ch]"
	applyLineEndings "*.ui"
	applyLineEndings "*.js"
	applyLineEndings "*.txt"
	applyLineEndings "*.in"
	applyLineEndings "*.cmake"
	applyLineEndings "*.md"
	applyLineEndings "*.sh"
else
	echo "dos2unix command not found, not changing file line endings"
fi

which chmod &> /dev/null
if [ $? -eq 0 ]; then
	# Test files
	chmod a-x .gitignore .gitmodules COPYING COPYING.LESSER
	applyFileAttributes "*.[ch]pp" "a-x"
	applyFileAttributes "*.[ch]" "a-x"
	applyFileAttributes "*.ui" "a-x"
	applyFileAttributes "*.js" "a-x"
	applyFileAttributes "*.txt" "a-x"
	applyFileAttributes "*.in" "a-x"
	applyFileAttributes "*.cmake" "a-x"
	applyFileAttributes "*.md" "a-x"
	
	# Binary files
	applyFileAttributes "*.sh" "a+x"
	applyFileAttributes "*.bat" "a+x"
	applyFileAttributes "*.exe" "a+x"
	applyFileAttributes "*.dll" "a+x"
else
	echo "chmod command not found, not changing file attributes"
fi
