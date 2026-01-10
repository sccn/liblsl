#!/bin/bash
# =============================================================================
# Apple XCFramework Creation Script for LSL
# =============================================================================
# Creates an XCFramework from macOS, iOS, and iOS Simulator frameworks.
#
# Usage:
#   ./scripts/apple_create_xcframework.sh --macos <path> --ios <path> --ios-simulator <path> [--output <dir>]
#
# Environment Variables:
#   APPLE_CODE_SIGN_IDENTITY_APP - Code signing identity (default: "Developer ID Application")
#   LSL_VERSION                  - Version string for output filename (optional)
#
# Examples:
#   # Create XCFramework from all three platforms
#   ./scripts/apple_create_xcframework.sh \
#       --macos build-macOS/Frameworks/lsl.framework \
#       --ios build-iOS/Frameworks/lsl.framework \
#       --ios-simulator build-iOS-Simulator/Frameworks/lsl.framework
#
#   # Create XCFramework with custom output directory
#   ./scripts/apple_create_xcframework.sh \
#       --macos build-macOS/Frameworks/lsl.framework \
#       --ios build-iOS/Frameworks/lsl.framework \
#       --ios-simulator build-iOS-Simulator/Frameworks/lsl.framework \
#       --output package/
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
SIGN_IDENTITY="${APPLE_CODE_SIGN_IDENTITY_APP:-Developer ID Application}"
OUTPUT_DIR="."
MACOS_FRAMEWORK=""
IOS_FRAMEWORK=""
IOS_SIM_FRAMEWORK=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --macos)
            MACOS_FRAMEWORK="$2"
            shift 2
            ;;
        --ios)
            IOS_FRAMEWORK="$2"
            shift 2
            ;;
        --ios-simulator)
            IOS_SIM_FRAMEWORK="$2"
            shift 2
            ;;
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --identity)
            SIGN_IDENTITY="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 --macos <path> --ios <path> --ios-simulator <path> [--output <dir>]"
            echo ""
            echo "Options:"
            echo "  --macos <path>         Path to macOS framework"
            echo "  --ios <path>           Path to iOS device framework"
            echo "  --ios-simulator <path> Path to iOS simulator framework"
            echo "  --output <dir>         Output directory (default: current directory)"
            echo "  --identity <name>      Override code signing identity"
            echo ""
            echo "Environment Variables:"
            echo "  APPLE_CODE_SIGN_IDENTITY_APP - Code signing identity"
            echo "  LSL_VERSION                  - Version for output filename"
            exit 0
            ;;
        -*)
            echo "Unknown option: $1"
            exit 1
            ;;
        *)
            echo "Unexpected argument: $1"
            exit 1
            ;;
    esac
done

# Validate inputs - need at least two frameworks
FRAMEWORK_COUNT=0
FRAMEWORK_ARGS=()

if [[ -n "$MACOS_FRAMEWORK" ]]; then
    if [[ ! -d "$MACOS_FRAMEWORK" ]]; then
        echo "Error: macOS framework not found at: $MACOS_FRAMEWORK"
        exit 1
    fi
    FRAMEWORK_ARGS+=("-framework" "$MACOS_FRAMEWORK")
    ((FRAMEWORK_COUNT++))
fi

if [[ -n "$IOS_FRAMEWORK" ]]; then
    if [[ ! -d "$IOS_FRAMEWORK" ]]; then
        echo "Error: iOS framework not found at: $IOS_FRAMEWORK"
        exit 1
    fi
    FRAMEWORK_ARGS+=("-framework" "$IOS_FRAMEWORK")
    ((FRAMEWORK_COUNT++))
fi

if [[ -n "$IOS_SIM_FRAMEWORK" ]]; then
    if [[ ! -d "$IOS_SIM_FRAMEWORK" ]]; then
        echo "Error: iOS Simulator framework not found at: $IOS_SIM_FRAMEWORK"
        exit 1
    fi
    FRAMEWORK_ARGS+=("-framework" "$IOS_SIM_FRAMEWORK")
    ((FRAMEWORK_COUNT++))
fi

if [[ $FRAMEWORK_COUNT -lt 2 ]]; then
    echo "Error: At least two frameworks required to create XCFramework"
    echo "Provide --macos, --ios, and/or --ios-simulator paths"
    exit 1
fi

# Determine version
if [[ -z "$LSL_VERSION" ]]; then
    # Try to get from macOS framework Info.plist
    if [[ -n "$MACOS_FRAMEWORK" ]]; then
        INFO_PLIST="$MACOS_FRAMEWORK/Versions/A/Resources/Info.plist"
        if [[ -f "$INFO_PLIST" ]]; then
            LSL_VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$INFO_PLIST" 2>/dev/null || echo "")
        fi
    fi
    # Try iOS framework
    if [[ -z "$LSL_VERSION" && -n "$IOS_FRAMEWORK" ]]; then
        INFO_PLIST="$IOS_FRAMEWORK/Info.plist"
        if [[ -f "$INFO_PLIST" ]]; then
            LSL_VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$INFO_PLIST" 2>/dev/null || echo "")
        fi
    fi
    if [[ -z "$LSL_VERSION" ]]; then
        LSL_VERSION="unknown"
    fi
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

XCFRAMEWORK_PATH="$OUTPUT_DIR/lsl.xcframework"
ZIP_PATH="$OUTPUT_DIR/lsl.xcframework.${LSL_VERSION}.zip"

echo "=== XCFramework Creation ==="
echo "Version: $LSL_VERSION"
echo "Identity: $SIGN_IDENTITY"
echo "Output: $XCFRAMEWORK_PATH"
echo ""
echo "Input frameworks:"
[[ -n "$MACOS_FRAMEWORK" ]] && echo "  macOS: $MACOS_FRAMEWORK"
[[ -n "$IOS_FRAMEWORK" ]] && echo "  iOS: $IOS_FRAMEWORK"
[[ -n "$IOS_SIM_FRAMEWORK" ]] && echo "  iOS Simulator: $IOS_SIM_FRAMEWORK"
echo ""

# Remove existing XCFramework
rm -rf "$XCFRAMEWORK_PATH"

# Create XCFramework
echo "Creating XCFramework..."
xcodebuild -create-xcframework \
    "${FRAMEWORK_ARGS[@]}" \
    -output "$XCFRAMEWORK_PATH"

# Sign the XCFramework
echo ""
echo "Signing XCFramework..."
codesign --force --deep --sign "$SIGN_IDENTITY" "$XCFRAMEWORK_PATH"

# Verify signature
echo ""
echo "Verifying XCFramework signature..."
codesign --verify --verbose --deep --strict "$XCFRAMEWORK_PATH"

# Create zip archive
echo ""
echo "Creating zip archive: $ZIP_PATH"
ditto -c -k --sequesterRsrc --keepParent "$XCFRAMEWORK_PATH" "$ZIP_PATH"

echo ""
echo "=== XCFramework Complete ==="
echo "XCFramework: $XCFRAMEWORK_PATH"
echo "Zip archive: $ZIP_PATH"

# Export for use in CI
if [[ -n "$GITHUB_ENV" ]]; then
    echo "XCFRAMEWORK_PATH=$XCFRAMEWORK_PATH" >> "$GITHUB_ENV"
    echo "XCFRAMEWORK_ZIP=$ZIP_PATH" >> "$GITHUB_ENV"
fi
