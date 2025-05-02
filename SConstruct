#!/usr/bin/env python
import os
import sys

env = SConscript("godot-cpp/SConstruct")
env.Append(CPPPATH=["odin-sdk/include"])

if env["arch"] == "x86_64" or env["arch"] == "amd64":
    arch = "x86_64"
elif env["arch"] == "arm64" or env["arch"] == "aarm64":
    arch = "aarch64"
elif env["arch"] == "universal" and env["platform"] == "macos":
    arch = "universal"
else:
    print("Unsupported architecture: {}".format(env["arch"]))
    sys.exit(1)

if env["platform"] == "windows":
    env.Append(CPPDEFINES=["PLATFORM_WINDOWS"])
    env.Append(LIBPATH=["odin-sdk/bin/windows-" + arch])
    env.Append(LIBS=["odin"])
elif env["platform"] == "macos":
    env.Append(CPPDEFINES=["PLATFORM_MACOS"])
    env.Append(LINKFLAGS=[
        "-Wl,-rpath,'@loader_path'",
        "-Wl,-install_name,'@rpath/libodin.macos.template_debug.universal.dylib'",
    ])

    if arch == "universal":
        # First, create universal binary for the SDK library
        env.Command("bin/libodin.dylib", ["odin-sdk/bin/macos-x86_64/libodin.dylib", "odin-sdk/bin/macos-aarch64/libodin.dylib"],
                   "lipo -create $SOURCES -output $TARGET")
        
        # Add the library path and name
        env.Append(LIBPATH=["bin"])
        env.Append(LIBS=["odin"])
    else:
        env.Append(LIBPATH=["odin-sdk/bin/macos-" + arch])
        env.Append(LIBS=["odin"])
elif env["platform"] == "linux":
    env.Append(CPPDEFINES=["PLATFORM_LINUX"])
    env.Append(LIBPATH=["odin-sdk/bin/linux-" + arch])
    env.Append(LIBS=["odin"])
    env.Append(LINKFLAGS=["-Wl,-rpath,'$ORIGIN'"])
else:
    print("Unsupported platform: {}".format(env["platform"]))
    sys.exit(1)

if env["platform"] == "windows":
    env.Command("bin/odin.dll", "odin_sdk/bin/windows-" + arch + "/odin.dll", Copy("$TARGET", "$SOURCE"))
elif env["platform"] == "macos":
    if arch == "universal":
        # Universal binary is already created above
        pass
    else:
        env.Command("bin/libodin.dylib", "odin-sdk/bin/macos-" + arch + "/libodin.dylib", Copy("$TARGET", "$SOURCE"))
else:
    env.Command("bin/libodin.so", "odin_sdk/bin/linux-" + arch + "/libodin.so", Copy("$TARGET", "$SOURCE"))

sources = Glob("src/odin_extension/*.cpp")

if env["platform"] == "windows":
    library = env.SharedLibrary(
        "bin/libodin{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources
    )
elif env["platform"] == "macos":
    library = env.SharedLibrary(
        "bin/libodin{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources
    )
else:  # linux
    library = env.SharedLibrary(
        "bin/libodin{}{}".format(env["suffix"], env["SHLIBSUFFIX"]),
        source=sources
    )

Default(library)