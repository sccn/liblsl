[![GitHub Actions Status](https://github.com/sccn/liblsl/workflows/C%2FC++%20CI/badge.svg)](https://github.com/sccn/liblsl/actions)
[![Travis Build Status](https://travis-ci.org/sccn/liblsl.svg?branch=master)](https://travis-ci.org/sccn/liblsl)
[![Azure Build Status](https://dev.azure.com/labstreaminglayer/liblsl/_apis/build/status/sccn.liblsl?branchName=master)](https://dev.azure.com/labstreaminglayer/liblsl/_build/latest?definitionId=1&branchName=master)
[![DOI](https://zenodo.org/badge/123265865.svg)](https://zenodo.org/badge/latestdoi/123265865)

# Lab Streaming Layer library

The lab streaming layer is a simple all-in-one approach to streaming experiment
data between applications in a lab, e.g. instrument time series, event markers,
audio, and so on. For more information, please read the
[online documentation](https://labstreaminglayer.readthedocs.io)

These repository is for the core library: `liblsl`

## Getting and using liblsl

The most up-to-date instructions to use liblsl are in the
[quick start online documentation](https://labstreaminglayer.readthedocs.io/info/getting_started.html).

You might also be interested in
[apps to connect to recording equipment](https://labstreaminglayer.readthedocs.io/info/supported_devices.html)
and the [LabRecorder](https://github.com/labstreaminglayer/App-LabRecorder) to record streams to disk.

To retrieve the latest liblsl release, you have a few options.

Precompiled packages are uploaded

- to the [Release page](https://github.com/sccn/liblsl/releases)
- the [Anaconda cloud](https://anaconda.org/conda-forge/liblsl), install with `conda install -c conda-forge liblsl`

liblsl is also available via the following package managers:

- [vcpkg](https://vcpkg.io)
- [Conan](https://conan.io/center/liblsl)
- [homebrew](https://brew.sh/) via `brew install labstreaminglayer/tap/lsl`

If you cannot find a liblsl for you via any of the above methods, then fear not because for most users it is simple to build.

## Building liblsl

To compile the library yourself from source,
please follow the [online documentation](https://labstreaminglayer.readthedocs.io/dev/lib_dev.html).

For single board computers running linux, you can also try
`standalone_compilation_linux.sh`.

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
* Unit tests are in the [`testing/`](testing/) directory

To connect an application to the lab streaming layer:
* Include the header for your language (`lsl_c.h` for C, `lsl_cpp.h for C++`)
  (automatically done when using CMake) or get
  [bindings for your preferred language](https://github.com/sccn/labstreaminglayer/tree/master/LSL)
* Make sure that the library file (`liblsl.so`/`liblsl.dylib`/`lsl.dll`) is found by your application. 
  On Windows, it should be enough to put it in the same folder as your executable.
  When building a Windows app, also make sure that the `lsl.lib` file is visible
  to your build environment.
* To provide data, create a new streaminfo to describe your stream and create a new outlet with that info. 
  Push samples into the outlet as your app produces them. Destroy the outlet when you're done.
* To receive data, resolve a stream that matches your citeria (e.g. name or type), which gives you a 
  streaminfo and create a new inlet with that streaminfo. Pull samples from the inlet. 
  Destroy the inlet when you're done.
* Have a look at the example sources in the
  [examples/ folder](https://github.com/labstreaminglayer/App-Examples)

The library and example applications are licensed under the MIT license.  
The library uses code that is licensed under the Boost software license.

# Acknowledgements

The original version of this software was written at the Swartz Center for Computational Neuroscience, UCSD. This work was funded by the Army Research Laboratory under Cooperative Agreement Number W911NF-10-2-0022 as well as through NINDS grant 3R01NS047293-06S1.

# Citing liblsl

[![DOI](https://zenodo.org/badge/123265865.svg)](https://zenodo.org/badge/latestdoi/123265865)

Information about versioning: https://help.zenodo.org/#versioning
