# Release checklist

## Update version number in
- `include/lsl_c.h`, `LSL_COMPILE_HEADER_VERSION`
- `CMakeLists.txt` in `project()`
- `src/common.h`, `LSL_LIBRARY_VERSION`
- `.appveyor.yml`, `version` and `deploy.version`

## Build binaries:
- Appveyor: Windows x86, x64, Ubuntu 16.04 x64, 18.04 x64
- Travis: Ubuntu 14.04 x64, OS X x64

