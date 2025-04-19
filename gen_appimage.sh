#!/usr/bin/env bash
# Generate an appimage for linux

# Command to get the absolute path
selfFolderPath="`cd "${BASH_SOURCE[0]%/*}"; pwd -P`/"
 
# Download linux deploy and the qt plugin
wget -nc https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
wget -nc https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
chmod +x linuxdeploy*.AppImage
 
# Setup environment and build
./setup_fresh_env.sh
./gen_cmake.sh --release
cmake --build ./_build_linux_x64_makefiles_release --config Release --parallel 4

# Generate AppImage
QT_LIB_PATH=/usr/lib/qt6/lib
LD_LIBRARY_PATH=$selfFolderPath/_build_linux_x64_makefiles_release/lib:$QT_LIB_PATH NO_STRIP=1 ./linuxdeploy-x86_64.AppImage --appdir AppDir --executable ./_build_linux_x64_makefiles_release/src/Hive -i ./resources/Hive.png --create-desktop-file --output appimage --plugin qt -l /usr/lib/x86_64-linux-gnu/libpcap.so.1.10.1
