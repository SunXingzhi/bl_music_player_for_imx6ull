# !/bin/bash

set -e	# Stop program after error occured.
# Usage: 
# Format: ./build.sh ARCH=<arch>(arm, host) [Optional: USE_CMAKE or USE_MAKE, DEFAUT:USE_CMAKE]
# If the optional item is chosen, there could be some arguments of cmake or make depend on your target.


# Function defination

# echo with stage
# arg1: stage_name(function name or current processing), 
# arg2: information to echo
secho(){
	if [ -n "$1" ]; then
		STAGE="$1:"
	else
		STAGE=""
	fi
	echo "$STAGE	$2"
}


#default situation
CROSS_COMPILE=false
ARCH="host"
COMPILE_TOOL="cmake"
ARCH_SET=false
USE_NINJA=""
EXTRA_ARGS=()	# Array including ninja, specified-tool-chain file, etc
CROSS_COMPILE_FILE_PATH="/home/q1325/Projects/IMX6ULL_learn/buildroot/output/host/share/buildroot/toolchainfile.cmake"


# Generate the cmake args: -DXXX=...
add_tool_args(){
	# Get the first args: prefix
	PREFIX=${1:-}
	index=0

	for i in "$@"
	do
		if [[ -n $i && $index != 0 ]]; then
			echo "Add args: "$PREFIX$i""
			EXTRA_ARGS+=("$PREFIX$i")
			
		fi
		index=$((index+1))
	done

	# echo "Add: "${EXTRA_ARGS[@]}"."
}

# Check the arguments
for i in "$@"	  
do
	case "$i" in
	ARCH=*)
		ARCH="${i#ARCH=}"; ARCH="${ARCH:-host}"
		secho "check" "ARCH is $ARCH."
		;;
	CMTOOL=*)
		COMPILE_TOOL="${i#CMTOOL=}"
		secho "check" "Compile Tool is $COMPILE_TOOL"
		;;
	USE_NINJA=*)
		if [[ ${i#USE_NINJA=} == "1" ]]; then
			GENERATOR="Ninja"
			
		else
			GENERATOR="Unix Makefiles"
		fi
		secho "check" "Generator set to ${GENERATOR}"
		;;
	TOOL_FILE=*)
		# TODO Here should be a check for the provided toolchain file. Supposed that it's correct.
		if [[ -f ${i#TOOL_FILE=} ]]; then
			CROSS_COMPILE_FILE_PATH=${i#TOOL_FILE=}
		fi
		
		secho "check" "Tool file set to ${CROSS_COMPILE_FILE_PATH}"
		;;
	# If chosen the venv path, it seems that open the defconfig
	VENV_PATH=*)

		if [[ -d ${i#VENV_PATH=} ]]; then
			# set the absolute path
			VENV_PATH="${i#VENV_PATH=}"
			VENV_PATH=$(cd "$VENV_PATH" && pwd)
			secho "check" "Venv path set to ${VENV_PATH}"

			# open the venv for using the config tools, such as menuconfig defconfig etc.
		else
			secho "check" "Venv path is invalid."
		fi
		
		;;
	*)
		echo "未知参数: $i"
		;;
	esac
done

# add the generator for building
add_tool_args "-G" "$GENERATOR"


# Set the tool chain and build path.
if [[ $ARCH == "arm" ]]; then
	COMPILER_TOOL_PATH="/usr/local/arm/15_2_arm-none-linux-gnueabihf/bin"
	COMPILER_NAME="arm-none-linux-gnueabihf-*"
	TARGET_BUILD_DIR="build_arm"
	add_tool_args "-D" "CMAKE_TOOLCHAIN_FILE=$CROSS_COMPILE_FILE_PATH"
else
	COMPILER_TOOL_PATH="/usr/bin"
	COMPILER_NAME="*"
	TARGET_BUILD_DIR="build_host"
	CROSS_COMPILE_FILE_PATH="None"
fi


# Confirm the build information
BUILD_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
secho "Confirm" "Chosen arch:		$ARCH, 
		Compile tools:		$COMPILE_TOOL, 
		Compiler path:		$COMPILER_TOOL_PATH/$COMPILER_NAME,
		Build path:		$BUILD_PATH/$TARGET_BUILD_DIR,
		Tool chain file:	$CROSS_COMPILE_FILE_PATH,
		Build args		"${EXTRA_ARGS[@]}""


# Set the defconfig
# activate the python venv
source "$VENV_PATH/bin/activate" env

# Start config
# ========debug=========
# delete the space in the header of the EXTRA_ARGS

secho "Build" "cmake -B \"$BUILD_PATH/$TARGET_BUILD_DIR\" \"${EXTRA_ARGS[@]}\" -S \"$BUILD_PATH\" "
echo "DEBUG: EXTRA_ARGS = ${EXTRA_ARGS[@]}"
cmake -B "$BUILD_PATH/$TARGET_BUILD_DIR" "${EXTRA_ARGS[@]}" -S "$BUILD_PATH"
# ======================

secho "Build" "Build finished."

# Start compile
secho "Compile" "Start to compile..."
if [[ $COMPILE_TOOL=="cmake" ]]; then
	cmake --build "$BUILD_PATH/$TARGET_BUILD_DIR"

fi

secho "Compile" "Finished."


echo "Scripts finished."