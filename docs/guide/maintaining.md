# Maintaining package ports

Several package managers carry liblsl. They live in other repositories and must be updated by
hand after each release, so they tend to lag behind. When a release changes the build (options,
dependencies, installed files), check each port.

| Channel | Port | Update by |
| --- | --- | --- |
| conda-forge | [conda-forge/liblsl-feedstock](https://github.com/conda-forge/liblsl-feedstock) | The conda-forge bot opens a version-bump PR after a release; review the recipe if the build changed. |
| vcpkg | [microsoft/vcpkg `ports/liblsl`](https://github.com/microsoft/vcpkg/tree/master/ports/liblsl) | PR updating `vcpkg.json` and `portfile.cmake` (version, hash, options). |
| Conan | [conan-center-index `recipes/liblsl`](https://github.com/conan-io/conan-center-index/tree/master/recipes/liblsl) | PR adding the version to `config.yml` and `all/conandata.yml`; adapt `conanfile.py` if the build changed. |
| Homebrew | [labstreaminglayer/homebrew-tap](https://github.com/labstreaminglayer/homebrew-tap) | Update the formula's URL and checksum. |

Things the build offers package maintainers:

- `BUILD_SHARED_LIBS` selects a shared or static liblsl; `LSL_BUILD_STATIC` overrides it.
- `LSL_FETCH_PUGIXML=OFF` builds against the packaged pugixml instead of downloading it, for
  sandboxed builds without network access. `LSL_BUNDLED_BOOST=OFF` uses packaged Boost headers.
- `LSL_FRAMEWORK=OFF` installs a plain `liblsl.dylib` on macOS, which is what Homebrew and conda ship.
- A `CMAKE_INSTALL_RPATH` passed on the command line is kept as is.
- `LSL_BUNDLE_DEPENDENCIES=OFF` (in `LSLCMake.cmake`) stops applications from copying liblsl and
  Qt into their install tree when the package manager provides them.
- `LSL_VERSION_OVERRIDE` sets the version when building from an archive without git tags.

The CMake package (`find_package(LSL)`), `lsl.pc`, the installed layout and the way
applications link liblsl are tested on every pull request by the *Install and link* workflow
(`.github/workflows/install-and-link.yml`, using `testing/consumer/`). Changes that keep it
green should not break a port's consumers.
