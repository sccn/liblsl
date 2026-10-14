# Building liblsl

Most users don't need to build liblsl: see [Getting liblsl](getting.md). Build it yourself to
change it, to target a platform without a release (embedded Linux, Android, iOS), or to build it
inside your own project with [FetchContent](linking.md#cmake-build-liblsl-with-your-project-fetchcontent).

## Requirements

- **CMake 3.23 or newer.** CI also builds with Debian 12's CMake 3.25.
- **A C++17 compiler:** GCC, Clang/AppleClang, MSVC (Visual Studio 2019 or newer) or MinGW-w64.
- **Git**, to derive the version number and to download pugixml.
- **Network access while configuring**, unless you use a system pugixml
  (`LSL_FETCH_PUGIXML=OFF`). Boost is bundled (a renamed subset in `lslboost/`) and asio and
  loguru are vendored in `thirdparty/`.

## Quick start

```sh
git clone https://github.com/sccn/liblsl.git
cd liblsl
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=install
cmake --build build --config Release -j
cmake --install build --config Release
install/bin/lslver        # prints the library version and build info
```

IDEs with CMake support (CLion, Visual Studio, VS Code, Qt Creator) open the folder directly;
set the same options in their CMake settings.

The install folder contains:

| Path | Contents |
| --- | --- |
| `bin/` | `lslver`; on Windows also `lsl.dll` |
| `lib/` | `liblsl.so*`, `liblsl.dylib` or `lsl.lib` (`liblsl.a` for static builds) |
| `include/` | `lsl_c.h`, `lsl_cpp.h` and `lsl/*.h` |
| `lib/cmake/lsl/` | the CMake package used by `find_package(LSL)` |
| `lib/pkgconfig/lsl.pc` | pkg-config file (Linux, macOS) |
| `Frameworks/lsl.framework` | macOS framework builds; `lib/` and `include/` then link into it |

`lib` may be `lib64` or `lib/<multiarch>` depending on the Linux distribution
(CMake's [GNUInstallDirs](https://cmake.org/cmake/help/latest/module/GNUInstallDirs.html)).

## Build options

Set options with `-D<option>=<value>` when configuring.

| Option | Default | Meaning |
| --- | --- | --- |
| `LSL_BUILD_STATIC` | `OFF` (`ON` if `BUILD_SHARED_LIBS` is `OFF`) | Build a static library. Its symbols stay hidden, so programs linking it don't re-export liblsl's API. |
| `LSL_FRAMEWORK` | `ON` on Apple platforms | Build `lsl.framework` instead of `liblsl.dylib`. |
| `LSL_OPTIMIZATIONS` | `ON` | Extra optimizations; link-time optimization for shared builds only. |
| `LSL_FETCH_PUGIXML` | `ON` | Download pugixml and compile it into liblsl. `OFF` uses an installed pugixml (`find_package(pugixml)`). |
| `LSL_BUNDLED_BOOST` | `ON` | Use the bundled Boost subset. `OFF` uses system Boost headers. |
| `LSL_DEBUGLOG` | `ON` for `Debug` builds | Verbose debug logging. |
| `LSL_UNITTESTS` | `OFF` | Build the unit tests (`LSL_BENCHMARKS` adds benchmarks). |
| `LSL_TOOLS` | `OFF` | Build experimental test tools. |
| `LSL_INSTALL` | `ON` | Generate install rules. Off automatically when liblsl is a subproject. |
| `LSL_UNIXFOLDERS` | `ON` | Unix layout (`bin/`, `lib/`, `include/`). `OFF` puts everything in one `LSL/` folder. |
| `CMAKE_INSTALL_FRAMEWORK_DIR` | `Frameworks` | Where the macOS framework goes; relative to the prefix, or absolute (e.g. `/Library/Frameworks`). |
| `LSL_WINVER` | `0x0601` | Minimum Windows version (`_WIN32_WINNT`). |
| `LSL_VERSION_OVERRIDE` | | Force the version (e.g. `1.18.0b1`) instead of reading the git tag. |
| `LSL_FORCE_FANCY_LIBNAME` | `OFF` | Name the library `liblsl64`/`liblsl32`, with a `-debug` suffix. Rarely needed. |
| `LSL_LEGACY_CPP_ABI` | `OFF` | Include the old C++ ABI. Don't use unless you know you need it. |

The library version comes from the nearest `v*` git tag reachable from the checked-out commit. Without a git
checkout (e.g. a source archive) the fallback version in `CMakeLists.txt` is used.

## Platform notes

### Linux

The quick start works as is. `cmake --build build --target package` creates `.deb` and
`.tar.gz` packages with CPack.

To cross-compile for 64-bit or 32-bit ARM (Raspberry Pi, Jetson, ...), install the cross
compiler and use the toolchain files in `cmake/toolchains/`:

```sh
sudo apt install g++-aarch64-linux-gnu
cmake -S . -B build-arm64 -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake
```

Building directly on a Raspberry Pi also works.

### macOS

liblsl builds `lsl.framework` by default; add `-DLSL_FRAMEWORK=OFF` for `liblsl.dylib`.
For a universal (Intel + Apple Silicon) build, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`,
and use the default fetched pugixml (system packages are usually single-architecture).
Installing a framework also installs `lib/liblsl.dylib` and `include/` symlinks into it, plus
`lib/pkgconfig/lsl.pc`, so tools that don't know about frameworks can use it.

Release frameworks are signed and notarized by `scripts/apple_*.sh` in CI; a local build is not.

### Windows (Visual Studio)

Choose the architecture with `-A x64`, `-A Win32` or `-A ARM64`. The Visual Studio generators
build several configurations, so pass `--config Release` when building and installing.

### Windows (MinGW)

In an [MSYS2](https://www.msys2.org) shell (e.g. UCRT64 or CLANGARM64):

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Programs built this way need the MinGW runtime DLLs next to them on other machines
(`LSL_install_mingw_runtime`, see [Using liblsl](linking.md#lslcmake-helpers)). On
CLANGARM64 the unit tests crash in a toolchain thread-local-storage limitation; the library
itself works.

### iOS and iPadOS

Use CMake's built-in iOS support (iOS 15 or newer):

```sh
# Device
cmake -S . -B build-ios -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_OSX_ARCHITECTURES=arm64
# Simulator
cmake -S . -B build-ios-sim -G Xcode -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_OSX_SYSROOT=iphonesimulator \
  "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
```

Build and install each, then combine them (with a macOS framework) into an xcframework with
`scripts/apple_create_xcframework.sh`. `scripts/build_apple_frameworks.sh` does all of it.

### Android

Use the NDK's toolchain file, as Android Studio's Gradle build does:

```sh
cmake -S . -B build-android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21
```

In an Android Studio project, add liblsl to the app's `CMakeLists.txt` with
[FetchContent](linking.md#cmake-build-liblsl-with-your-project-fetchcontent) (or
`add_subdirectory`) and link `LSL::lsl`; Gradle passes the ABI and API level. Below API 24,
Android has no `getifaddrs`, so liblsl cannot list the network interfaces and sends discovery
queries through the system's default interface only (list others under `multicast.Interfaces`
in the [configuration](configuration.md)). Apps that use LSL need the `INTERNET` permission, and for discovery on Wi-Fi a
multicast lock (`WifiManager.MulticastLock`).

## Running the tests

```sh
cmake -S . -B build -DLSL_UNITTESTS=ON
cmake --build build -j
ctest --test-dir build/testing -C Release --output-on-failure
```

`lsl_test_internal` checks internal components; `lsl_test_exported` uses the public API like
an application. Both use [Catch2](https://github.com/catchorg/Catch2) and accept its options,
e.g. `lsl_test_exported "[sync]"` to run one group. Some tests need working networking
(multicast on the loopback interface) and can fail in restricted containers.
