# Getting liblsl

There are three ways to get liblsl, in order of how closely they track this repository:

1. **Build it as part of your project** with CMake's `FetchContent`. You always get exactly
   the version you ask for, built with your compiler and settings. See
   [Using liblsl in your project](linking.md#cmake-build-liblsl-with-your-project-fetchcontent).
2. **Download a release** from the
   [GitHub releases page](https://github.com/sccn/liblsl/releases). These are built and
   tested by this repository's CI for every release (`<arch>` is `x86_64`, `arm64`, `arm` or
   `i386`; releases before 1.18 say `amd64` instead of `x86_64`):

   | Platform | Asset | Contents |
   | --- | --- | --- |
   | Windows (x64, x86, ARM64) | `liblsl-<version>-Win_<arch>.zip` | `lsl.dll`, import library, headers, CMake package |
   | Ubuntu (several releases; x64, arm64, armhf) | `liblsl-<version>-<codename>_<arch>.deb` / `.tar.gz` | shared library, headers, CMake package, pkg-config file |
   | macOS (universal) | `liblsl-<version>-Darwin-universal.pkg` | signed, notarized installer that puts `lsl.framework` in `/Library/Frameworks` |
   | macOS, iOS, iOS simulator | `lsl.xcframework.<version>.zip` | signed xcframework for Xcode projects (see [iOS](building.md#ios-and-ipados)) |
   | Windows MinGW (UCRT x64, ARM64), from 1.18 | `liblsl-<version>-MinGW-<msys>-<DLL\|static>.zip` | MinGW-built library, for MSYS2 toolchains |

3. **Install it with a package manager.** These are maintained separately from this
   repository and are sometimes several releases behind; check the version before relying
   on a recent fix:

   - [conda-forge](https://anaconda.org/conda-forge/liblsl): `conda install -c conda-forge liblsl`
   - [vcpkg](https://vcpkg.io): `vcpkg install liblsl`
   - [Conan](https://conan.io/center/liblsl)
   - [Homebrew](https://brew.sh): `brew install labstreaminglayer/tap/lsl` (a shared
     library with headers, a CMake package and a pkg-config file, like on Linux)

All of these install the same CMake package (`find_package(LSL)` → target `LSL::lsl`), so
the [linking instructions](linking.md) are the same whichever you choose.

You may not need liblsl directly at all: most LSL users work through an application
(for example LabRecorder) or a language binding (Python, MATLAB, C#, ...) that bundles it.
See the {doc}`LSL documentation <lsl:index>` for the wider ecosystem.

Package-manager maintainers: see [Maintaining package ports](maintaining.md).
