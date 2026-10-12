# Usage:
# cmake -DCMAKE_TOOLCHAIN_FILE=cmake/user_cross_compile_setup.cmake -B build -S .
# cmake --build build -j$(nproc)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(tools /usr/local/arm/15_2_arm-none-linux-guneabihf)
set(CMAKE_C_COMPILER ${tools}/bin/arm-none-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/arm-none-linux-gnueabihf-g++)

# If necessary, set STAGING_DIR
# if not work, please try(in shell command): export STAGING_DIR=/home/ubuntu/Your_SDK/out/xxx/openwrt/staging_dir/target
#set(ENV{STAGING_DIR} "/home/ubuntu/Your_SDK/out/xxx/openwrt/staging_dir/target")

