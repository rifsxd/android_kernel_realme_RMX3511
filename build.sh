#!/bin/bash

# Display usage help
function usage() {
    echo "Usage: $0 [config|compile|dtc|clean]"
    exit 1
}

# Check if an argument is passed
if [ -z "$1" ]; then
    usage
fi

# Check if setup.env exists and source it
if [ -f "setup.env" ]; then
    source "setup.env"
else
    echo "Error: setup.env not found in the current directory."
    exit 1
fi

# Define the base command with shared options
base_command='make ARCH="arm64" O="out" CC="clang" CLANG_TRIPLE="aarch64-linux-gnu-" CROSS_COMPILE="aarch64-linux-android-" CROSS_COMPILE_ARM32="arm-linux-androideabi-" LD="ld.lld" AR="llvm-ar" NM="llvm-nm" OBJCOPY="llvm-objcopy" OBJDUMP="llvm-objdump" READELF="llvm-readelf" OBJSIZE="llvm-size" STRIP="llvm-strip" LDGOLD="aarch64-linux-gnu-ld.gold" LLVM_AR="llvm-ar" LLVM_DIS="llvm-dis" BSP_BUILD_ANDROID_OS="y" BSP_BUILD_FAMILY="qogirl6"'

# Execute the respective command based on the input
case "$1" in
    clean)
        eval "rm -rf out kernel_log.log"
        ;;
    config-dev)
        eval "$base_command RMX3511_athena_defconfig"
        ;;
    config-stock)
        eval "$base_command RMX3511_defconfig"
        ;;
    compile)
        eval "$base_command -j$(nproc) 2>&1 | tee ./out/kernel_log.log"
        ;;
    dtc)
        eval "$base_command -j$(nproc) dtbs"
        ;;
    *)
        usage
        ;;
esac
