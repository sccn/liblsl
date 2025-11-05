# CMake toolchains

To build locally for ARM64:
```bash
cmake -S . -B build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/aarch64-linux-gnu.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm64
```

To build locally for ARMv7:
```bash
cmake -S . -B build-armv7 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/arm-linux-gnueabihf.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-armv7
```

It is also possible to cross-compile from other hosts but the toolchain files may need to be adjusted accordingly.

## Testing Cross-Compiled Binaries

With QEMU:

```bash
# Install QEMU
sudo apt-get install qemu-user-static

# Run ARM64 binary
qemu-aarch64-static -L /usr/aarch64-linux-gnu ./build-arm64/your-binary

# Run ARMv7 binary
qemu-arm-static -L /usr/arm-linux-gnueabihf ./build-armv7/your-binary
```

## NVIDIA Jetson

The ARM64 build (`aarch64-linux-gnu.cmake`) binaries should be compatible with NVIDIA Jetson.
