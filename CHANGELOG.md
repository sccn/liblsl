# Changes for liblsl 1.16

* add: optional, minimal header-only replacement for Boost.Serialization (Tristan Stenner)
* add: extensible `lsl_create_inlet_ex()` for high-precision buffer lengths (Chadwick Boulay)
* change: replace Boost.Uuid, Boost.Random and Boost.Thread with built-in functions (Tristan Stenner)
* change: replace Boost.Asio with upstream Asio (Tristan Stenner)
* change: update bundled Boost to 1.78 (Tristan Stenner)
* change: allow building against system Boost again (@chausner)
* change: speed up resolving a fixed number of streams (Tristan Stenner)
* change: reduce Asio operation overhead (Tristan Stenner)
* change: IPv6 is enabled by default on macOS (Tristan Stenner)
* change: share io contexts for IPv4+IPv6 services (Tristan Stenner)
* **change**: send resolve requests from all local network interfaces (Tristan Stenner)
* fix: fix a minor memory leak when closing streams (Tristan Stenner)

# Changes for liblsl 1.15.2

* fix: bump artifact / soname version

# Changes for liblsl 1.15.1

* change: update Boost / Boost.Asio to 1.75 (Tristan Stenner)
* change: replace Boost.UUID generator
* fix: work around faulty MinGW code generation for thread local variables (Tristan Stenner, Tobias Herzke)


# Changes for liblsl 1.15

* add: thread-safe `lsl_last_error()` function, returns a description of the last error
  (https://github.com/sccn/liblsl/pull/75, Tristan Stenner)
* change: object handles in the C++ API use smart pointers internally.
  The smart pointer to the object can be retrieved with the `handle()` function
* **change**: The `stream_info` copy constructor creates a shallow copy.
  Use `stream_info::clone()` for the previous behavior (deep copy).
* change: replace more Boost libraries with C++11 counterparts (Tristan Stenner)
* change: speed up `push_chunk_*` calls by caching the sampling rate (Chadwick Boulay)
* change: postprocessing parameters are reset after dis- and re-enabling postprocessing (Tristan Stenner)
* fix: postprocessing now works even when flushing samples (Tristan Stenner)
* fix: pulling strings with embedded NULL chars now works (Tristan Stenner, Chadwick Boulay)
* fix: various build system and deployment fixes (Chadwick Boulay, Tristan Stenner)
* fix: building with MinGW now works (Tobias Herzke)
* fix: `max_buflen` documentation and edge cases (Chadwick Boulay)
* fix: preliminary support for deploying Qt6 apps with CMake (Chadwick Boulay, Tristan Stenner)
* fix: packages for Ubuntu 18.04 no longer depend on the unavailable libgcc_s1 package
  (Tristan Stenner, many thanks to Tobias Herzke for reporting the issue)


# Changes for liblsl 1.14

* **change**: CMake doesn't set `CMAKE_INSTALL_PREFIX`and `CMAKE_BUILD_TYPE` by default unless `LSL_COMFY_DEFAULTS` is set (Tristan Stenner)
* fix: prevent race condition when two threads modify an overflowing buffer (https://github.com/sccn/liblsl/pull/71, Jérémy Frey)
* fix: improve latency and CPU usage (https://github.com/sccn/liblsl/pull/71, Jérémy Frey)
* fix: various build system fixes and improvments (Chadwick Boulay, Tristan Stenner)
* added: enumerate network interfaces on startup, for now unused. Requires Android SDK >=26 (Tristan Stenner)
* added: lsl_inlet_flush() to drop all outstanding samples from an inlet to avoid the pull_ overhead in realtime applications (Tristan Stenner)
* change: the binary names are changed so that the OS resolver (e.g. `find_library(lsl)`) searches
  for the correct library. Windows: liblsl32.dll / liblsl64.dll -> lsl.dll, Unix: liblsl.so. (Tristan Stenner)
* change: the lsl_c.h header is now split into several header files (Tristan Stenner)
* change: refactor logging, add `log.file` config option (Tristan Stenner)
* change: replace various Boost libraries with C++11 counterparts (Tristan Stenner)
* change: liblsl requires a compiler with C++11 support, MSVC 2015 on Windows

# Changes for liblsl 1.13 (released 2020-01-30)

* fix: avoid an infinite loop if no port can be bound to (Tristan Stenner, discovered by Maximilian Kraus)
* add: unit tests to test basic operations on CI systems (Tristan Stenner, Matthew Grivich)
* fix: Fix for the sometimes appearing "no clock offsets found"-bug
* add: the path to the lsl_api.cfg can now be specified via the environment variable LSLAPICFG
* fix: updates to the included Boost, fixing Android compilation and reducing the number of warnings (Tristan Stenner)
* fix: patch Boost.Thread to avoid latency spikes on Windows (Tristan Stenner)
* add: lsl_stream_info_matches_query() to test a resolved stream against a query (Tristan Stenner)
* add: lsl_library_info() function returning some information about the dll/so file (Tristan Stenner)
* add: Raspberry Pi cross compilation support (Chadwick Boulay)
* fix: Boost.Serialization objects are only defined to fix compilation on ARM (Tristan Stenner)
* change: added move-semantics to most C++ API classes (Tristan Stenner)
* change: move error handling from C API to C++ implementation (Tristan Stenner)
* fix: Fix possible string stream corruption on 32bit platforms (Tristan Stenner)
* change: add some Continuous Integration and packaging configuration (Tristan Stenner)
* change: remove the option to build liblsl against system Boost (Tristan Stenner)
* fix: prevent undefined shift (Tristan Stenner)
* fix: several fixes to export macros (Tristan Stenner)
* change: lots of improvements on OS X (Chadwick Boulay)
* change: a lot of work on making Qt work on OS X (Chadwick Boulay)
* change: override default timestamps with lsl timestamps (Zechari Tempesta)
* change: Switched the build system to CMake (Tristan Stenner, Chadwick Boulay, David Medine, Matthew Grivich)
* fix: Fix for ipv6 link-local addresses on Linux. Previously would get a "Invalid argument" error. (Matthew Grivich)
* fix: fix several memory handling issues (Matthew Grivich)
* fix: Added an optional time_correction prototype to return uncertainty and remote_time as well as offset. (Matthew Grivich)
* fix: merged memory leak fixes (thanks to xloem)

# Changes for liblsl 1.12

* Added support for iOS. (Matthew Grivich; de3f1318)
* changed constructor of lsl::xml_element to have default (David Medine; fd6a87b7)
* Add support for gcc 5 / MinGW (BorisMansencal)
* Fixed warning in GCC 4.9: 'lsl::stream_info' has a field 'lsl::stream_info::obj' whose type uses the anonymous namespace 1> class stream_info { (Matthew Grivich; 072563f5)
* Added optional time-stamp post-processing to the LSL inlet (Christian Kothe; d29ebde8)
* fixed broken relative paths (Giso Grimm; 1396ce6e)
* automatic version number extraction (Giso Grimm; a3e912ad)
* version number extraction and new location (Giso Grimm; 56f210a2)

Changes for liblsl 1.11

* Updated C/C++ examples to conform to XDF meta-data spec (per Luca Mussi). (Christian Kothe; 61d83de6)
* pugixml 1.0->1.7 . This fixes a build error in OS X. Also tested in Win 10 binaries with no apparent problems. (Chadwick Boulay; 067e1330)
* several changes to the build system and include files (Matthew Grivich, Chadwick Boulay)
* Fixed remaining references to old documentation. (Kyu Crane; ecf9cd09)
* Some fixes and updates here and there. (Christian Kothe; 65fa64e2)
* Fixed bug in push_chunk_multiplexed (Christian Kothe; 89339d08)
* Fixed regression in samples_available() (Christian Kothe; 5d0659da)
* Delete ._stream_info_impl.cpp (Christian Kothe; 92bfd87c)
* Updated consumer_queue::pop_sample() (Christian Kothe; 22662cb7)
* Fixed consumer_queue::pop_sample() to respect the timeout. (Steven Boswell; 678995dd)
* Fixed some memory leaks in the C# interop layer. (Steven Boswell; 06684ec6)
* Now multicasting can specify the local IP address to bind to. (Steven Boswell; 04a397e4)
* attempting to strip binaries (David Medine; bc216cc2)

Changes for liblsl 1.10

[Here a lot of changes happened that aren't included yet]

Changes for distribution 1.0.10  (library version 0.91)
* fix: in the C++ interface, the template pull_chunk() that fills a vector of time stamps did not clear this vector initially
       (no recompile necessary, just a header fix)
* change: simplified the rule for deciding which chunk size to use when both the outlet and the inlet have a preference:
          now the inlet's preference overrides the one of the outlet, if present.

Changes for distribution 1.0.18 (library version 0.92)
* fix: rebuild Linux and Mac libraries with most boost symbols stripped off (reducing the conflict of name clashes during dynamic linking, e.g. in MATLAB).
* Adding support for constructing inlets with unresolved stream_info's (if they are fully specified at construction).

Changes for distribution 1.0.19 (library version 0.93)
* removed the entire boost static library dependency across Win/Linux/Mac (instead now using the relevant .cpp files directly). 
  This simplifies the build process and reduces the symbol pollution on Linux/Mac.
* Documented compatibility with pugixml in C API.
* Documented the timeout in resolve_all better in C API.

Changes for distribution 1.0.20 (library version 1.00) -- internal code review.
* Added a continuous_resolver class that allows to resolve streams in the background.
* Added ability to query receive buffer size.
* Extended the error reporting in the C API (some functions now return error conditions).
* Added some extra argument checking in the stream_info constructor (negative channel count / sample rate, empty name).

* Upgraded from boost 1.47.0 to boost 1.50.0, and from portable_archive 4.2 to portable_archive 5.0.

* Improved code documentation and slightly refactored some of the library code for better readability / comprehensibility.
  - renamed some internal variables
  - enhanced documentation
  - switched largely to boost.chrono based timing instead of boost.date_time (new standard)
  - following boost synchronization guidelines more strictly (preferentially using the simplest primitive that gets the job done)
  - made the stream_inlet more tolerant to protocol errors at the sender side (threads do not exit but try to reconnect until successful).
  - using incomplete struct types in the LSL C API to prevent accidental type confusions.
  - simplified (and robustified) the data and info fetching threads in the inlet.
  - reduced the amount of data copying when pushing samples into the library
  - the IPv4 and IPv6 stacks in the outlet now run on two separate threads to improve robustness in case an OS implements a protocol badly
  - improved efficiency of stream_info accessors (now return const references instead of copies)
  - now using boost.container.flat_set in the send_buffer instead of std.set (more efficient)

* Fixed a bug that prevented the use of LSL_FOREVER in the C API under 32-bit Mac (and possibly 32-bit Linux, too).
* Fixed some potential hangups in the inlet when the connection gets lost irrecoverably 
  (affected info(), time_correction() and pull_sample()).
* Fixed several missing exception handlers for the C API wrappers (increasing robustness in case of bad errors).
* Fixed a minor bug when resolving streams from a list of known peers (used the wrong time constant).
* Fixed 2 cases where strings were allocated incorrectly in the liblsl C API (lsl_get_xml() and lsl_pull_sample_str()).
* Fixed a bug where -0.0 would be transmitted as +0.0 (fixed in the portable_archive upgrade).
* Fixed several destructors that could have thrown exceptions and thus caused application termination.
* Fixed a bug that prevented the use of minimum buffer sizes that were larger than the lower capacity bound (then 4096).

Changes for distribution 1.0.21 (library version 1.02) -- after extensive stress testing.
* Added a stress-testing app under testing/StressTest
* Relaxed the timing of reconnect attempts by the inlet a bit.
* Fixed the handling of the chunk size when passed in at construction of an inlet or outlet (oversight)
* Fixed a rare abstract function call or access violation when cancelling resolve attempts
* Fixed a relatively rare access violation in the TCP server shutdown
* Fixed a relatively rare access violation in the stream outlet shutdown
* Fixed an occasional "not a socket" error in the TCP server shutdown
* Fixed handling of some unusual parameter settings in the inlet (buffer size=0)

Library version 1.03
* Added support for Windows XP (required more tolerance in case of missing IPv6 support)
* Fixed a case where an inlet could spam an outlet with rapid reconnect requests
* Rebuilt against a sufficiently old Linux to run out-of-the-box for most distributions

Library version 1.03.1
* Made the resolver tolerant w.r.t. invalid hostnames in the KnownPeers config variable

Library version 1.03.2
* Moved a definition inside the C++ header file to make SWIG happy (which is needed for certain features in the Python wrapper to be enabled)
* pylsl should now work out of the box on all Windows platforms and all recent Python versions

Library Version 1.03.3
* renamed internal use of boost namespace into lslboost to avoid link-time symbol conflicts
* Added the ability to push/pull length-delimeted string values in the C API (lsl_push_sample_buf / lsl_pull_sample_buf)
  in addition to 0-terminated
* Added an experimental Java wrapper

Library Version 1.04
* Added a preprocessor switch to toggle between using the system-supplied boost or the shrink-wrapped version (in external/lslboost)
* Added documentation on how to get an updated boost distribution included with liblsl (and added a script to automate that)

Library Version 1.05
* Suppressed a spurious warning about non-matching query ids

Library Version 1.10 (mostly throughput optimizations)
* Added the ability to allocate ports outside the default port range once if it is exhausted (configurable, enabled by default).
* Replaced consumer_queue implementation by a lock-free single-producer/single-consumer queue 
* Minor speed improvements in data_receiver: Removed a slow check in pull_sample() and calling local_clock() only every k'th iteration in data_thread().
* Changed the data layout of the sample class for faster allocation/deallocation.
* Speeded up the assignment/retrieval functions of the sample class (uses memcpy where appropriate).
* Added a factory class for fast allocation/deallocation of samples.
* Implemented a faster data transmission protocol (1.10) for samples, as alternative to the baseline protocol (1.00).
* Implemented a new version of protocol negotiation between tcp_server and data_receiver, using an HTTP-like request/response style.
* Fixed some potential rare hang bugs during shutdown of inlets and outlets (deadlocks/livelocks)
* Added the ability to transmit multiplexed chunks to the C and C++ APIs
* Minor improvements to documentation
* Added clean cross-platform Python and C# wrappers (pylsl.py and LSL.cs)
