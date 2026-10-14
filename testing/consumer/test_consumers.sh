#!/usr/bin/env bash
# Builds and installs liblsl, then checks the ways applications get and link it:
#   - the installed lslver tool runs (install RPATH / DLL placement)
#   - DESTDIR (staged) installs work
#   - find_package(LSL) against the install
#   - FetchContent of the source tree
#   - pkg-config (where available)
#   - macOS frameworks: the framework's own CMake dir, and plain clang -F
#
# Usage: testing/consumer/test_consumers.sh shared|static [extra liblsl CMake args...]
# Environment:
#   WORK=<dir>     scratch directory (default: ./consumer-test)
#   PREFIX=system  install to CMake's default prefix (with $SUDO) instead of $WORK/prefix
#   SUDO=sudo      prefix for commands that write to the system prefix
set -euo pipefail

LINKAGE=${1:?usage: $0 shared|static [cmake args...]}
shift
case "$(uname -s)" in
	MINGW*|MSYS*|CYGWIN*) WINDOWS=1 ;;
	*) WINDOWS=0 ;;
esac
# Windows CMake needs C:/... paths, not the shell's /c/...
native_pwd() { if [[ $WINDOWS == 1 ]]; then pwd -W; else pwd; fi; }

SRC=$(cd "$(dirname "$0")/../.." && native_pwd)
CONSUMER="$SRC/testing/consumer"
WORK=${WORK:-$(native_pwd)/consumer-test}
SUDO=${SUDO:-}
rm -rf "$WORK" && mkdir -p "$WORK"
STATIC=OFF; SHARED=ON
[[ $LINKAGE == static ]] && STATIC=ON && SHARED=OFF

step() { echo; echo "=== $*"; }
# Runs an executable from a single-config (dir/x) or multi-config (dir/Release/x) build.
run() {
	local dir=$1 name=$2 exe
	for exe in "$dir/$name" "$dir/$name.exe" "$dir/Release/$name" "$dir/Release/$name.exe"; do
		if [[ -f $exe ]]; then echo "+ $exe"; "$exe"; return; fi
	done
	echo "error: $name not found in $dir" >&2
	return 1
}

step "Build and install liblsl ($LINKAGE)"
if [[ ${PREFIX:-} == system ]]; then
	prefix_args=()
else
	PREFIX="$WORK/prefix"
	prefix_args=("-DCMAKE_INSTALL_PREFIX=$PREFIX")
fi
cmake -S "$SRC" -B "$WORK/lsl-build" -DCMAKE_BUILD_TYPE=Release -DLSL_BUILD_STATIC=$STATIC \
	${prefix_args[@]+"${prefix_args[@]}"} "$@"
cmake --build "$WORK/lsl-build" --config Release -j 4
$SUDO cmake --install "$WORK/lsl-build" --config Release
if [[ ${PREFIX:-} == system ]]; then
	PREFIX=$(sed -n 's/^CMAKE_INSTALL_PREFIX:PATH=//p' "$WORK/lsl-build/CMakeCache.txt")
	[[ $(uname -s) == Linux ]] && $SUDO ldconfig
fi
run "$PREFIX/bin" lslver

if [[ $WINDOWS == 0 ]]; then
	step "DESTDIR install"
	# (with $SUDO as the build dir may now belong to root)
	$SUDO env DESTDIR="$WORK/destdir" cmake --install "$WORK/lsl-build" --config Release
	# Relative RPATHs mean the staged copy runs too.
	run "$WORK/destdir$PREFIX/bin" lslver
fi

# Windows has no RPATH: put the installed lsl.dll on PATH for the find_package consumer.
if [[ $WINDOWS == 1 ]]; then PATH="$(cygpath -u "$PREFIX/bin"):$PATH"; export PATH; fi

step "find_package(LSL)"
cmake -S "$CONSUMER" -B "$WORK/find_package" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$PREFIX"
cmake --build "$WORK/find_package" --config Release
run "$WORK/find_package/bin" consumer_cpp
run "$WORK/find_package/bin" consumer_c

step "FetchContent (BUILD_SHARED_LIBS=$SHARED)"
# Static is requested the way package managers do it, through BUILD_SHARED_LIBS.
cmake -S "$CONSUMER" -B "$WORK/fetchcontent" -DCMAKE_BUILD_TYPE=Release "-DLSL_SOURCE_DIR=$SRC" \
	-DBUILD_SHARED_LIBS=$SHARED "$@"
cmake --build "$WORK/fetchcontent" --config Release
run "$WORK/fetchcontent/bin" consumer_cpp
run "$WORK/fetchcontent/bin" consumer_c
if [[ -f "$WORK/fetchcontent/CPackConfig.cmake" ]]; then
	echo "error: liblsl set up CPack for the parent project" >&2
	exit 1
fi

pcfile=$(find "$PREFIX" -name lsl.pc -path '*pkgconfig*' 2>/dev/null | head -n1 || true)
if command -v pkg-config >/dev/null && [[ -n $pcfile ]] && [[ $WINDOWS == 0 ]]; then
	step "pkg-config ($pcfile)"
	PKG_CONFIG_PATH=$(dirname "$pcfile"); export PKG_CONFIG_PATH
	pcflags=(); [[ $STATIC == ON ]] && pcflags=(--static)
	# shellcheck disable=SC2046
	c++ -std=c++17 "$CONSUMER/main.cpp" -o "$WORK/pkgconfig_cpp" $(pkg-config ${pcflags[@]+"${pcflags[@]}"} --cflags --libs lsl)
	LD_LIBRARY_PATH="$(pkg-config --variable=libdir lsl)" run "$WORK" pkgconfig_cpp
fi

framework="$PREFIX/Frameworks/lsl.framework"
if [[ -d $framework ]]; then
	step "Framework: find_package via the framework's CMake dir"
	cmake -S "$CONSUMER" -B "$WORK/framework_cmake" -DCMAKE_BUILD_TYPE=Release \
		"-DLSL_DIR=$framework/Resources/CMake"
	cmake --build "$WORK/framework_cmake"
	run "$WORK/framework_cmake/bin" consumer_cpp

	step "Framework: clang -F"
	printf '#include <lsl/lsl_cpp.h>\nint main() { return lsl::library_version() > 0 ? 0 : 1; }\n' \
		>"$WORK/framework_main.cpp"
	c++ -std=c++17 "$WORK/framework_main.cpp" -o "$WORK/framework_clang" \
		-F "$PREFIX/Frameworks" -framework lsl -Wl,-rpath,"$PREFIX/Frameworks"
	run "$WORK" framework_clang
fi

step "All consumer checks passed ($LINKAGE)"
