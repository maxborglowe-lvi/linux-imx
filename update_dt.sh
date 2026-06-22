#!/bin/bash
source /opt/fslc-xwayland/3.3/environment-setup-cortexa53-crypto-fslc-linux
export LDFLAGS=

BUILD_DIR=${BUILD_DIR:-$(pwd)}

if [[ "${REGEN_CONFIG:-0}" == "1" ]]; then
    make O="$BUILD_DIR" ARCH=arm64 defconfig imx8_var_defconfig my_defconfig.config
else
    # Silently merge any config changes without updating timestamp if unchanged
    make O="$BUILD_DIR" ARCH=arm64 olddefconfig
fi

make O="$BUILD_DIR" ARCH=arm64 -j$(nproc)
make O="$BUILD_DIR" ARCH=arm64 freescale/imx8mp-var-dart-dt8mcustomboard-lvi.dtb

./install.sh