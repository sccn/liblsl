# Using liblsl in your project

liblsl has a C API (`#include <lsl_c.h>`) and a header-only C++ wrapper around it
(`#include <lsl_cpp.h>`). Both come from the same library, so any way of linking below works
for C and C++ programs.

The CMake package is the best-supported route: it carries the include directories, the
compile definitions and, for static builds, every dependency, so your project only names one
target. The same package is installed by release archives, package managers and `cmake --install`.

## CMake: an installed liblsl

```cmake
find_package(LSL REQUIRED)
target_link_libraries(myapp PRIVATE LSL::lsl)
```

If CMake doesn't find it on its own, point it at the install prefix (the folder that contains
`lib/` and `include/`) when configuring:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/liblsl     # extracted release, or your own install
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix lsl)"  # Homebrew
```

A static liblsl is used the same way. Its CMake package looks up what liblsl itself needs
(threads, a system pugixml if liblsl was built against one, and the Windows socket libraries)
and passes it on, so you don't link anything extra.

`cmake --install` of a macOS framework build also puts the CMake package in `lib/cmake/lsl`, so
the `CMAKE_PREFIX_PATH` above works for it too. The release `.pkg` and older installs only have
it inside the framework; for those, configure with
`-DLSL_DIR=/Library/Frameworks/lsl.framework/Resources/CMake` (or the framework's location).

## CMake: build liblsl with your project (FetchContent)

```cmake
include(FetchContent)
FetchContent_Declare(liblsl
    GIT_REPOSITORY https://github.com/sccn/liblsl.git
    GIT_TAG v1.18.0     # a release tag
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(liblsl)

target_link_libraries(myapp PRIVATE LSL::lsl)
```

liblsl is then configured and built with your compiler and settings. Good to know:

- **Shared or static.** liblsl builds a shared library by default. Set `LSL_BUILD_STATIC ON`
  before `FetchContent_MakeAvailable`, or set `BUILD_SHARED_LIBS OFF` (liblsl follows it when
  it is defined).
- **macOS.** liblsl builds an `lsl.framework` by default (`LSL_FRAMEWORK`); set
  `LSL_FRAMEWORK OFF` for a plain `liblsl.dylib`.
- **Network at configure time.** liblsl downloads [pugixml](https://pugixml.org) while
  configuring. For offline or sandboxed builds use a system pugixml with `LSL_FETCH_PUGIXML OFF`
  (see [Building liblsl](building.md#build-options)).
- **No side effects.** As a subproject, liblsl adds no install rules, tests or packaging targets
  to your project.

## Shipping your application

A program linked to a *shared* liblsl needs to find it at run time:

- **Windows:** `lsl.dll` must be next to your `.exe` (or on `PATH`). In CMake, copy it with
  `$<TARGET_RUNTIME_DLLS:myapp>`, or use `LSL_install_liblsl` (below).
- **Linux:** a liblsl in a system library folder is found automatically. If you ship liblsl
  next to your program, give the program an RPATH such as `$ORIGIN/../lib`.
- **macOS:** liblsl's install name starts with `@rpath`, so your program needs an RPATH entry
  for the folder that holds `lsl.framework` or `liblsl.dylib` (CMake adds one for the build
  tree; for installed apps use e.g. `@executable_path/../Frameworks`). In an app bundle, put
  `lsl.framework` in `Contents/Frameworks`.

A *static* liblsl has none of these concerns. It also keeps its symbols to itself: a program or
shared library that links it does not export liblsl's API.

### LSLCMake helpers

liblsl installs `LSLCMake.cmake` next to its CMake package with helpers for applications:

```cmake
find_package(LSL REQUIRED)
include("${LSL_DIR}/LSLCMake.cmake")   # with FetchContent: "${liblsl_SOURCE_DIR}/cmake/LSLCMake.cmake"

LSL_configure_rpath()                  # before creating targets
add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE LSL::lsl)
install(TARGETS myapp RUNTIME DESTINATION bin)
LSL_install_liblsl(DESTINATION bin FRAMEWORK_DESTINATION Frameworks)
```

| Function | What it does |
| --- | --- |
| `LSL_configure_rpath()` | Sets relative RPATHs so installed programs find a liblsl shipped next to them. |
| `LSL_install_liblsl(DESTINATION … [FRAMEWORK_DESTINATION …])` | Copies `lsl.dll`, `liblsl.so*`, `liblsl.dylib` or `lsl.framework` into your install tree, from an installed or a FetchContent liblsl. |
| `LSL_install_mingw_runtime(DESTINATION …)` | MinGW only: copies the compiler's runtime DLLs. |
| `LSL_deploy_qt(TARGET … DESTINATION …)` | Runs `windeployqt` / `macdeployqt` on a Qt application. |
| `LSL_codesign(TARGET … DESTINATION … ENTITLEMENTS …)` | Ad-hoc signs a macOS app or program, e.g. with the multicast entitlement. |
| `LSL_get_target_arch()`, `LSL_get_os_name()` | Set `LSL_ARCH` (e.g. `x86_64`, `arm64`) and `LSL_OS` for package file names. |

Package managers that ship liblsl and Qt themselves set `LSL_BUNDLE_DEPENDENCIES OFF`, which
turns `LSL_install_liblsl`, `LSL_deploy_qt` and `LSL_codesign` into no-ops.

## pkg-config

Linux installs and macOS installs made with `cmake --install` (including Homebrew) include `lsl.pc`:

```sh
c++ main.cpp $(pkg-config --cflags --libs lsl) -o myapp
c++ main.cpp $(pkg-config --static --cflags --libs lsl) -o myapp   # static liblsl
```

On macOS the flags include the RPATH that liblsl's `@rpath` install name needs. A framework
install also provides `lib/liblsl.dylib` and `include/` links into the framework, so the same
flags work there. If liblsl is installed outside the default search path, set
`PKG_CONFIG_PATH=<prefix>/lib/pkgconfig`.

## Other build systems and plain compilers

- Add `<prefix>/include` to the include path and link `-llsl` (`lsl.lib` with MSVC).
- **Windows, static liblsl:** define `LIBLSL_STATIC` when compiling your code. With a shared
  liblsl, define nothing.
- **MSVC:** including the headers already asks the linker for `lsl.lib`
  (`#pragma comment(lib, ...)`); define `LSLNOAUTOLINK` to turn that off.
- **macOS framework:** compile and link with `-F <dir containing lsl.framework> -framework lsl`
  and include the headers as `#include <lsl/lsl_cpp.h>`.

## Xcode, iOS and iPadOS

Add `lsl.xcframework` from a [release](https://github.com/sccn/liblsl/releases) to your target
with *Embed & Sign*, and `#include <lsl/lsl_cpp.h>` (or `lsl_c.h`). Stream discovery uses UDP
multicast, so iOS apps need the `com.apple.developer.networking.multicast` entitlement
(requested from Apple per app ID) and an `NSLocalNetworkUsageDescription` entry in
`Info.plist`. To build the frameworks yourself see [Building liblsl](building.md#ios-and-ipados).

## Examples

The [`examples/`](https://github.com/sccn/liblsl/tree/dev/examples) folder has small C and C++
programs (sending and receiving data, markers, chunks, metadata) with a `CMakeLists.txt`
that uses `find_package(LSL)`. `testing/consumer/` is the minimal project CI uses to check every
way of linking described on this page.
