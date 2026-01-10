#!/bin/bash
# =============================================================================
# Build Script for liblsl Apple Frameworks
# =============================================================================
# Builds, signs, and optionally packages/notarizes LSL frameworks for Apple
# platforms (macOS, iOS, iOS Simulator).
#
# This is a convenience wrapper that orchestrates the build process and calls
# the individual signing/packaging scripts.
#
# Usage:
#   ./scripts/build_apple_frameworks.sh [macos|ios|xcframework|all] [--notarize]
#
# Environment Variables:
#   BUILD_TYPE                    - Build type (default: Release)
#   APPLE_CODE_SIGN_IDENTITY_APP  - App signing identity (default: "Developer ID Application")
#   APPLE_CODE_SIGN_IDENTITY_INST - Installer signing identity (default: "Developer ID Installer")
#   APPLE_DEVELOPMENT_TEAM        - Team ID for notarization
#   APPLE_NOTARIZE_USERNAME       - Apple ID for notarization
#   APPLE_NOTARIZE_PASSWORD       - App-specific password for notarization
#
# Examples:
#   # Build macOS framework only (ad-hoc signed)
#   APPLE_CODE_SIGN_IDENTITY_APP="-" ./scripts/build_apple_frameworks.sh macos
#
#   # Build all frameworks and create XCFramework
#   ./scripts/build_apple_frameworks.sh all
#
#   # Build macOS and notarize installer package
#   ./scripts/build_apple_frameworks.sh macos --notarize
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/cmake-build-apple"
INSTALL_DIR="$PROJECT_ROOT/install"
PACKAGE_DIR="$PROJECT_ROOT/package"

# Default configuration
BUILD_TYPE="${BUILD_TYPE:-Release}"
export APPLE_CODE_SIGN_IDENTITY_APP="${APPLE_CODE_SIGN_IDENTITY_APP:-Developer ID Application}"
export APPLE_CODE_SIGN_IDENTITY_INST="${APPLE_CODE_SIGN_IDENTITY_INST:-Developer ID Installer}"

# Parse arguments
PLATFORM="${1:-all}"
DO_NOTARIZE=false
shift || true

while [[ $# -gt 0 ]]; do
    case $1 in
        --notarize)
            DO_NOTARIZE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [macos|ios|xcframework|all] [--notarize]"
            echo ""
            echo "Platforms:"
            echo "  macos       Build macOS universal framework"
            echo "  ios         Build iOS device and simulator frameworks"
            echo "  xcframework Create XCFramework from existing builds"
            echo "  all         Build all platforms and create XCFramework"
            echo ""
            echo "Options:"
            echo "  --notarize  Submit macOS installer for notarization"
            echo ""
            echo "Environment Variables:"
            echo "  BUILD_TYPE                    - Release or Debug (default: Release)"
            echo "  APPLE_CODE_SIGN_IDENTITY_APP  - Code signing identity"
            echo "  APPLE_CODE_SIGN_IDENTITY_INST - Installer signing identity"
            echo "  APPLE_DEVELOPMENT_TEAM        - Team ID for notarization"
            echo "  APPLE_NOTARIZE_USERNAME       - Apple ID for notarization"
            echo "  APPLE_NOTARIZE_PASSWORD       - App-specific password"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Show configuration
echo "=== LSL Apple Framework Build ==="
echo "Platform: $PLATFORM"
echo "Build Type: $BUILD_TYPE"
echo "Build Dir: $BUILD_DIR"
echo "Install Dir: $INSTALL_DIR"
echo "Code Sign Identity (App): $APPLE_CODE_SIGN_IDENTITY_APP"
echo "Code Sign Identity (Inst): $APPLE_CODE_SIGN_IDENTITY_INST"
echo "Notarize: $DO_NOTARIZE"
echo ""

# Build macOS framework
build_macos() {
    echo ""
    echo "========================================="
    echo "Building macOS Framework (Universal)"
    echo "========================================="

    rm -rf "$BUILD_DIR/macos"
    mkdir -p "$BUILD_DIR/macos"

    cmake -B "$BUILD_DIR/macos" -S "$PROJECT_ROOT" \
        -G Xcode \
        -DLSL_FRAMEWORK=ON \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=10.15 \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/macos"

    cmake --build "$BUILD_DIR/macos" --config "$BUILD_TYPE" -j
    cmake --build "$BUILD_DIR/macos" --config "$BUILD_TYPE" --target install

    echo ""
    echo "Signing macOS framework..."
    "$SCRIPT_DIR/apple_codesign.sh" "$INSTALL_DIR/macos/Frameworks/lsl.framework" --platform macos

    if [[ "$DO_NOTARIZE" == true ]]; then
        echo ""
        echo "Creating and notarizing macOS installer..."
        mkdir -p "$PACKAGE_DIR"
        NOTARIZE_ARG="--notarize"
    else
        NOTARIZE_ARG=""
    fi

    "$SCRIPT_DIR/apple_package_notarize.sh" "$INSTALL_DIR/macos/Frameworks/lsl.framework" \
        --output "$PACKAGE_DIR" $NOTARIZE_ARG
}

# Build iOS device framework
build_ios_device() {
    echo ""
    echo "========================================="
    echo "Building iOS Device Framework"
    echo "========================================="

    rm -rf "$BUILD_DIR/ios-device"
    mkdir -p "$BUILD_DIR/ios-device"

    cmake -B "$BUILD_DIR/ios-device" -S "$PROJECT_ROOT" \
        -G Xcode \
        -DLSL_FRAMEWORK=ON \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_TOOLCHAIN_FILE="$PROJECT_ROOT/cmake/ios.toolchain.cmake" \
        -DPLATFORM=OS64 \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/ios-device"

    cmake --build "$BUILD_DIR/ios-device" --config "$BUILD_TYPE" -j
    cmake --build "$BUILD_DIR/ios-device" --config "$BUILD_TYPE" --target install

    echo ""
    echo "Signing iOS device framework..."
    "$SCRIPT_DIR/apple_codesign.sh" "$INSTALL_DIR/ios-device/Frameworks/lsl.framework" --platform ios
}

# Build iOS simulator framework
build_ios_simulator() {
    echo ""
    echo "========================================="
    echo "Building iOS Simulator Framework"
    echo "========================================="

    rm -rf "$BUILD_DIR/ios-simulator"
    mkdir -p "$BUILD_DIR/ios-simulator"

    cmake -B "$BUILD_DIR/ios-simulator" -S "$PROJECT_ROOT" \
        -G Xcode \
        -DLSL_FRAMEWORK=ON \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_TOOLCHAIN_FILE="$PROJECT_ROOT/cmake/ios.toolchain.cmake" \
        -DPLATFORM=SIMULATOR64COMBINED \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR/ios-simulator"

    cmake --build "$BUILD_DIR/ios-simulator" --config "$BUILD_TYPE" -j
    cmake --build "$BUILD_DIR/ios-simulator" --config "$BUILD_TYPE" --target install

    echo ""
    echo "Signing iOS simulator framework..."
    "$SCRIPT_DIR/apple_codesign.sh" "$INSTALL_DIR/ios-simulator/Frameworks/lsl.framework" --platform ios-simulator
}

# Create XCFramework
create_xcframework() {
    echo ""
    echo "========================================="
    echo "Creating XCFramework"
    echo "========================================="

    XCFW_ARGS=()

    if [[ -d "$INSTALL_DIR/macos/Frameworks/lsl.framework" ]]; then
        XCFW_ARGS+=("--macos" "$INSTALL_DIR/macos/Frameworks/lsl.framework")
    fi

    if [[ -d "$INSTALL_DIR/ios-device/Frameworks/lsl.framework" ]]; then
        XCFW_ARGS+=("--ios" "$INSTALL_DIR/ios-device/Frameworks/lsl.framework")
    fi

    if [[ -d "$INSTALL_DIR/ios-simulator/Frameworks/lsl.framework" ]]; then
        XCFW_ARGS+=("--ios-simulator" "$INSTALL_DIR/ios-simulator/Frameworks/lsl.framework")
    fi

    if [[ ${#XCFW_ARGS[@]} -lt 4 ]]; then  # Each framework needs 2 args (--type and path)
        echo "Error: Need at least two frameworks to create XCFramework"
        echo "Build macos and/or ios first"
        exit 1
    fi

    mkdir -p "$PACKAGE_DIR"
    "$SCRIPT_DIR/apple_create_xcframework.sh" "${XCFW_ARGS[@]}" --output "$PACKAGE_DIR"
}

# Main execution
case "$PLATFORM" in
    "macos")
        build_macos
        ;;
    "ios")
        build_ios_device
        build_ios_simulator
        ;;
    "xcframework")
        create_xcframework
        ;;
    "all")
        build_macos
        build_ios_device
        build_ios_simulator
        create_xcframework
        ;;
    *)
        echo "Error: Unknown platform: $PLATFORM"
        echo "Use: macos, ios, xcframework, or all"
        exit 1
        ;;
esac

echo ""
echo "========================================="
echo "Build Complete!"
echo "========================================="
echo ""
echo "Output locations:"
if [[ -d "$INSTALL_DIR/macos/Frameworks/lsl.framework" ]]; then
    echo "  macOS Framework: $INSTALL_DIR/macos/Frameworks/lsl.framework"
fi
if [[ -d "$INSTALL_DIR/ios-device/Frameworks/lsl.framework" ]]; then
    echo "  iOS Device Framework: $INSTALL_DIR/ios-device/Frameworks/lsl.framework"
fi
if [[ -d "$INSTALL_DIR/ios-simulator/Frameworks/lsl.framework" ]]; then
    echo "  iOS Simulator Framework: $INSTALL_DIR/ios-simulator/Frameworks/lsl.framework"
fi
if [[ -d "$PACKAGE_DIR/lsl.xcframework" ]]; then
    echo "  XCFramework: $PACKAGE_DIR/lsl.xcframework"
fi
if ls "$PACKAGE_DIR"/*.pkg 1> /dev/null 2>&1; then
    echo "  Installer Package: $PACKAGE_DIR/*.pkg"
fi
