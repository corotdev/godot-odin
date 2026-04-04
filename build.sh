#!/bin/bash

if [ -z "$1" ]; then
  echo "Usage: $0 <debug|release> <platform>"
  exit 1
fi

if [ -z "$2" ]; then
  echo "Usage: $0 <debug|release> <platform>"
  exit 1
fi

# eventually take params
scons platform=$2 target=template_$1
mkdir -p demo/addons/odin_extension/bin

# if macos... assume universal. else assume x86_64
if [ "$2" == "macos" ]; then
  arch="universal"

  # Create universal binary using lipo
  lipo -create \
    odin-sdk/bin/macos-aarch64/libodin.dylib \
    odin-sdk/bin/macos-x86_64/libodin.dylib \
    -output demo/addons/odin_extension/bin/libodin.dylib

  if [ ! -f bin/libodin.macos.template_debug.universal.dylib ]; then
      echo "Extension not found, build it first"
      exit 1
  fi

  # Copy the extension library
  cp bin/libodin.macos.template_debug.universal.dylib demo/addons/odin_extension/bin/

  # Fix the rpath to look in the same directory
  install_name_tool -change "bin/libodin.macos.template_debug.universal.dylib" "@rpath/libodin.macos.template_debug.universal.dylib" demo/addons/odin_extension/bin/libodin.macos.template_debug.universal.dylib
  install_name_tool -add_rpath "@loader_path" demo/addons/odin_extension/bin/libodin.macos.template_debug.universal.dylib

  cp src/odin_extension/odin_extension.gdextension demo/addons/odin_extension/
elif [ "$2" == "linux" ]; then
  arch="x86_64"

  cp odin-sdk/bin/linux-x86_64/libodin.so demo/addons/odin_extension/bin/

  EXTENSION_LIB="bin/libodin.linux.template_$1.x86_64.so"

  if [ ! -f "$EXTENSION_LIB" ]; then
      echo "Extension not found: $EXTENSION_LIB"
      exit 1
  fi

  cp "$EXTENSION_LIB" demo/addons/odin_extension/bin/

  # Note: This requires 'patchelf' to be installed
  if command -v patchelf &> /dev/null; then
    patchelf --set-rpath '$ORIGIN' demo/addons/odin_extension/bin/$(basename "$EXTENSION_LIB")
  else
    echo "Warning: patchelf not found. You may need to install it or set LD_LIBRARY_PATH to run the extension."
  fi

else
  arch="x86_64"
fi

cp src/odin_extension/odin_extension.gdextension demo/addons/odin_extension/
