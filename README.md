[![GitHub Actions Status](https://github.com/sccn/liblsl/workflows/C%2FC++%20CI/badge.svg)](https://github.com/sccn/liblsl/actions)
[![DOI](https://zenodo.org/badge/123265865.svg)](https://zenodo.org/badge/latestdoi/123265865)

# Lab Streaming Layer library

The lab streaming layer (LSL) is a simple all-in-one approach to streaming experiment
data between applications in a lab, e.g. instrument time series, event markers,
audio, and so on. This repository is the core library, `liblsl`, which every LSL
application and language binding uses.

- **LSL in general** (what it is, supported devices, apps, language bindings, getting started):
  the [LSL documentation](https://labstreaminglayer.readthedocs.io).
- **liblsl itself**: the [liblsl documentation](https://labstreaminglayer.readthedocs.io/projects/liblsl/):
  - [Getting liblsl](https://labstreaminglayer.readthedocs.io/projects/liblsl/guide/getting.html): releases, package managers, building it with your project
  - [Using liblsl in your project](https://labstreaminglayer.readthedocs.io/projects/liblsl/guide/linking.html): CMake `find_package` / FetchContent, pkg-config, frameworks, iOS
  - [Building liblsl](https://labstreaminglayer.readthedocs.io/projects/liblsl/guide/building.html): options and platform notes (Linux, ARM, macOS, Windows, MinGW, iOS, Android)
  - [Configuration](https://labstreaminglayer.readthedocs.io/projects/liblsl/guide/configuration.html): `lsl_api.cfg`
  - [How liblsl works](https://labstreaminglayer.readthedocs.io/projects/liblsl/internals/overview.html): discovery, the data protocol, time synchronization, connection recovery
  - [API reference](https://labstreaminglayer.readthedocs.io/projects/liblsl/)

## Quick start

With CMake, add liblsl to your project and link the `LSL::lsl` target:

```cmake
include(FetchContent)
FetchContent_Declare(liblsl GIT_REPOSITORY https://github.com/sccn/liblsl.git GIT_TAG v1.18.0)
FetchContent_MakeAvailable(liblsl)
target_link_libraries(myapp PRIVATE LSL::lsl)
```

or use an installed liblsl with `find_package(LSL REQUIRED)`. Then include `lsl_cpp.h` (C++)
or `lsl_c.h` (C), and see the programs in [`examples/`](examples/).

## Design goals

The design goals of the library are:
a) The interface shall be as simple as possible, allowing programs or drivers to send 
   or receive data in just 3-5 lines of code.
b) The library should be available for a variety of languages (currently C, C++, Matlab, Python, Java) 
   and platforms (Windows, Mac OS X, Linux, 32/64 bit) and be fully interoperable between them.
c) Data transmission should work "out of the box", even across networks with no need to configure 
   IP addresses / hostnames and such (thanks to on-the-fly service discovery), also time synchronization 
   and failure recovery should work out of the box.
d) The library should be fully featured. It should cover the relevant streaming data formats incl. 
   multi-channel signals, regular/irregular sampling rate and the major channel data types 
   (int8, int16, int32, float, double, string) in a simple interface. Generic stream meta-data should 
   be supported. Advanced transmission features should be available if desired (but not in the way for 
   simple uses), including custom ways of chunking and buffering the data. It should be possible to 
   configure and tune the behavior of the library (e.g. networking features) via configuration files 
   in a way that is transparent to the applications.
e) Network and processor overhead should be reasonably low to not get in the way.

Package overview:
* The API headers are in the [`include/`](include/) directory.
* The library source code is in the [`src/`](src/) directory.
* Unit tests are in the [`testing/`](testing/) directory.
* The documentation sources are in [`docs/`](docs/).

To connect an application to the lab streaming layer:
* Include the header for your language (`lsl_c.h` for C, `lsl_cpp.h` for C++)
  or get a [binding for your preferred language](https://labstreaminglayer.readthedocs.io/info/language_wrappers.html)
* Make sure that the library file (`liblsl.so`/`liblsl.dylib`/`lsl.dll`) is found by your application. 
  On Windows, it should be enough to put it in the same folder as your executable.
  When building a Windows app, also make sure that the `lsl.lib` file is visible
  to your build environment.
* To provide data, create a new streaminfo to describe your stream and create a new outlet with that info. 
  Push samples into the outlet as your app produces them. Destroy the outlet when you're done.
* To receive data, resolve a stream that matches your citeria (e.g. name or type), which gives you a 
  streaminfo and create a new inlet with that streaminfo. Pull samples from the inlet. 
  Destroy the inlet when you're done.
* Have a look at the example sources in the [`examples/`](examples/) folder.

The library and example applications are licensed under the MIT license.  
The library uses code that is licensed under the Boost software license.

# Acknowledgements

The original version of this software was written at the Swartz Center for Computational Neuroscience, UCSD. This work was funded by the Army Research Laboratory under Cooperative Agreement Number W911NF-10-2-0022 as well as through NINDS grant 3R01NS047293-06S1.

# Citing liblsl

[![DOI](https://zenodo.org/badge/123265865.svg)](https://zenodo.org/badge/latestdoi/123265865)

Information about versioning: https://help.zenodo.org/#versioning
