# !/bin/bash
# Usage:
# Format: ./build.sh ARCH=<arch>(arm, host) [Optional: USE_CMAKE or USE_MAKE, DEFAUT:USE_CMAKE]
# If the optional item is chosen, there could be some arguments of cmake or make depend on your target.
# Config options:
#   CONFIG=<defconfig_path>  Use the given defconfig file to generate .config (e.g. CONFIG=configs/get_started.defconfig)
#   MENUCONFIG=1             Open the menuconfig interface (after loading defconfig if CONFIG= is also given)
#   If neither is given, reuse the existing .config; if no .config exists, open menuconfig.

BUILD_PATH="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cat "$BUILD_PATH/logo.txt"

if [ -z "$BASH_VERSION" ]; then
	echo "Please excute script with bash: bash build.sh" >&2
	exit 1
fi

set -e # Stop program after error occured.

# Function defination
# echo with stage
# arg1: stage_name(function name or current processing),
# arg2: information to echo
secho() {
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
CONFIG_FILE=""
MENUCONFIG=false
USE_NINJA=""
GENERATOR=""
EXTRA_ARGS=() # Array including ninja, specified-tool-chain file, etc
CROSS_COMPILE_FILE_PATH="/home/q1325/Projects/IMX6ULL_learn/buildroot/output/host/share/buildroot/toolchainfile.cmake"

# Generate the cmake args: -DXXX=...
add_tool_args() {
	# Get the first args: prefix
	PREFIX=${1:-}
	index=0

	for i in "$@"; do
		if [[ -n $i && $index != 0 ]]; then
			echo "Add args: "$PREFIX$i""
			EXTRA_ARGS+=("$PREFIX$i")

		fi
		index=$((index + 1))
	done

	# echo "Add: "${EXTRA_ARGS[@]}"."
}

# Check the arguments
for i in "$@"; do
	case "$i" in
	ARCH=*)
		ARCH="${i#ARCH=}"
		ARCH="${ARCH:-host}"
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
			secho "check" "Venv path is empty. It will create a new environment"
			VENV_PATH=""
		fi

		;;
	CONFIG=*)
		# Defconfig file used to generate the .config
		CONFIG_FILE="${i#CONFIG=}"
		if [[ -f $CONFIG_FILE ]]; then
			# convert to the absolute path before changing the working directory
			CONFIG_FILE="$(cd "$(dirname "$CONFIG_FILE")" && pwd)/$(basename "$CONFIG_FILE")"
			secho "check" "Defconfig file set to ${CONFIG_FILE}"
		else
			secho "check" "Defconfig file not found: ${CONFIG_FILE}" >&2
			exit 1
		fi
		;;
	MENUCONFIG=*)
		if [[ ${i#MENUCONFIG=} == "1" ]]; then
			MENUCONFIG=true
			secho "check" "MENUCONFIG enabled, will open the menuconfig interface."
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
echo "==========================================================="

secho "Confirm" "Chosen arch:		$ARCH, 
		Compile tools:		$COMPILE_TOOL, 
		Compiler path:		$COMPILER_TOOL_PATH/$COMPILER_NAME,
		Build path:		$BUILD_PATH/$TARGET_BUILD_DIR,
		Tool chain file:	$CROSS_COMPILE_FILE_PATH,
		Build args		"${EXTRA_ARGS[@]}""


# Get python environment
if  ! command -v python3 &>/dev/null ; then
	secho "Check" "python3 environment is required but not found." >&2
	exit 1
fi

if [[ -d "$VENV_PATH" ]]; then
	secho "Check" "Use the $VENV_PATH"
	"$VENV_PATH"/bin/python3 -m pip install kconfiglib pcpp
else
	secho "Check" "Failed to find the venv. Creating the python3 project environment. Use: python3"
	VENV_PATH="$BUILD_PATH/env"
	python3 -m venv "$VENV_PATH" && "$VENV_PATH"/bin/python3 -m pip install kconfiglib pcpp
	secho "Check" "Installation finished."
fi


# Start config
# Generate .config from defconfig / menuconfig, or reuse the existing one
cd "$BUILD_PATH"
# if [[ -f "CMakeCache.txt" && -n "$GENERATOR" ]]; then
# 	rm -f "CMakeCache.txt"
# 	secho "Config" "rm cmake cache for using generator."
# fi

if [[ -n "$CONFIG_FILE" ]]; then
	secho "Config" "Load defconfig: ${CONFIG_FILE}"
	"$VENV_PATH"/bin/defconfig "$CONFIG_FILE"
fi

if $MENUCONFIG; then
	secho "Config" "Opening menuconfig..."
	"$VENV_PATH"/bin/menuconfig
elif [[ -n "$CONFIG_FILE" ]]; then
	secho "Config" ".config generated from the defconfig."
elif [[ -f .config ]]; then
	secho "Config" "Reuse the existing .config. (Use CONFIG=<path> or MENUCONFIG=1 to change it)"
else
	secho "Config" "No .config found. Opening menuconfig to create one..."
	"$VENV_PATH"/bin/menuconfig
fi

secho "Build" "cmake -B \"$BUILD_PATH/$TARGET_BUILD_DIR\" \"${EXTRA_ARGS[@]}\" -S \"$BUILD_PATH\" "
cmake -B "$BUILD_PATH/$TARGET_BUILD_DIR" "${EXTRA_ARGS[@]}" -S "$BUILD_PATH"
secho "Build" "Build finished."

# Start compile
secho "Compile" "Start to compile..."
if [[ $COMPILE_TOOL=="cmake" ]]; then
	cmake --build "$BUILD_PATH/$TARGET_BUILD_DIR"

fi
secho "Compile" "Finished."

# Sync compile_commands.json to the project root so clangd picks up
# the correct (host or arm) compile flags automatically.
secho "clangd" "Sync $TARGET_BUILD_DIR/compile_commands.json -> project root"
cp "$BUILD_PATH/$TARGET_BUILD_DIR/compile_commands.json" "$BUILD_PATH/compile_commands.json"

secho "Script" "Finished."

# Send bin to nfs, which need a super user permission.
NFS_PATH="../../rootfs/nfs/root"
cp "$TARGET_BUILD_DIR/bin/lvglsim" 