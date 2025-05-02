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
else
  arch="x86_64"
fi


