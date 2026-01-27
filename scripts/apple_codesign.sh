#!/bin/bash
# =============================================================================
# Apple Code Signing Script for LSL Frameworks
# =============================================================================
# Signs macOS and iOS frameworks with the appropriate code signing identity.
#
# Usage:
#   ./scripts/apple_codesign.sh <framework_path> [--platform macos|ios]
#
# Environment Variables:
#   APPLE_CODE_SIGN_IDENTITY_APP  - Code signing identity (default: "Developer ID Application")
#                                   Set to "-" for ad-hoc signing (local development)
#
# Examples:
#   # Sign macOS framework (with hardened runtime and entitlements)
#   ./scripts/apple_codesign.sh install/Frameworks/lsl.framework --platform macos
#
#   # Sign iOS framework
#   ./scripts/apple_codesign.sh build-iOS/Frameworks/lsl.framework --platform ios
#
#   # Ad-hoc signing for local development
#   APPLE_CODE_SIGN_IDENTITY_APP="-" ./scripts/apple_codesign.sh install/Frameworks/lsl.framework
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
SIGN_IDENTITY="${APPLE_CODE_SIGN_IDENTITY_APP:-Developer ID Application}"
ENTITLEMENTS_FILE="${ENTITLEMENTS_FILE:-$PROJECT_ROOT/lsl.entitlements}"
PLATFORM="macos"

# Parse arguments
FRAMEWORK_PATH=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --platform)
            PLATFORM="$2"
            shift 2
            ;;
        --identity)
            SIGN_IDENTITY="$2"
            shift 2
            ;;
        --entitlements)
            ENTITLEMENTS_FILE="$2"
            shift 2
            ;;
        -h|--help)
            echo "Usage: $0 <framework_path> [--platform macos|ios] [--identity <identity>] [--entitlements <file>]"
            echo ""
            echo "Environment Variables:"
            echo "  APPLE_CODE_SIGN_IDENTITY_APP  - Code signing identity (default: 'Developer ID Application')"
            echo "  ENTITLEMENTS_FILE             - Path to entitlements file (default: lsl.entitlements)"
            exit 0
            ;;
        -*)
            echo "Unknown option: $1"
            exit 1
            ;;
        *)
            FRAMEWORK_PATH="$1"
            shift
            ;;
    esac
done

if [[ -z "$FRAMEWORK_PATH" ]]; then
    echo "Error: Framework path required"
    echo "Usage: $0 <framework_path> [--platform macos|ios]"
    exit 1
fi

if [[ ! -d "$FRAMEWORK_PATH" ]]; then
    echo "Error: Framework not found at: $FRAMEWORK_PATH"
    exit 1
fi

echo "=== Apple Code Signing ==="
echo "Framework: $FRAMEWORK_PATH"
echo "Platform: $PLATFORM"
echo "Identity: $SIGN_IDENTITY"
echo ""

# Determine the binary path within the framework
if [[ "$PLATFORM" == "macos" ]]; then
    # macOS framework: Versions/A/lsl
    BINARY_PATH="$FRAMEWORK_PATH/Versions/A/lsl"
    if [[ ! -f "$BINARY_PATH" ]]; then
        echo "Error: macOS framework binary not found at: $BINARY_PATH"
        exit 1
    fi

    # Check for entitlements file
    ENTITLEMENTS_ARG=""
    if [[ -f "$ENTITLEMENTS_FILE" ]]; then
        ENTITLEMENTS_ARG="--entitlements $ENTITLEMENTS_FILE"
        echo "Entitlements: $ENTITLEMENTS_FILE"
    else
        echo "Warning: Entitlements file not found at $ENTITLEMENTS_FILE"
        echo "         Signing without entitlements (may affect notarization)"
    fi
    echo ""

    # Sign the binary first (with hardened runtime for notarization)
    echo "Signing binary: $BINARY_PATH"
    codesign --force --deep --sign "$SIGN_IDENTITY" \
        --options runtime \
        $ENTITLEMENTS_ARG \
        "$BINARY_PATH"

    # Verify binary signature
    echo "Verifying binary signature..."
    codesign --verify --verbose --strict "$BINARY_PATH"

    # Sign the entire framework bundle
    echo ""
    echo "Signing framework bundle: $FRAMEWORK_PATH"
    codesign --force --deep --sign "$SIGN_IDENTITY" \
        --options runtime \
        $ENTITLEMENTS_ARG \
        "$FRAMEWORK_PATH"

elif [[ "$PLATFORM" == "ios" || "$PLATFORM" == "ios-simulator" ]]; then
    # iOS framework: lsl (no Versions directory)
    BINARY_PATH="$FRAMEWORK_PATH/lsl"
    if [[ ! -f "$BINARY_PATH" ]]; then
        echo "Error: iOS framework binary not found at: $BINARY_PATH"
        exit 1
    fi
    echo ""

    # Sign the binary (no hardened runtime for iOS)
    echo "Signing binary: $BINARY_PATH"
    codesign --force --deep --sign "$SIGN_IDENTITY" "$BINARY_PATH"

    # Verify binary signature
    echo "Verifying binary signature..."
    codesign --verify --verbose --strict "$BINARY_PATH"

    # Sign the entire framework bundle
    echo ""
    echo "Signing framework bundle: $FRAMEWORK_PATH"
    codesign --force --deep --sign "$SIGN_IDENTITY" "$FRAMEWORK_PATH"
else
    echo "Error: Unknown platform: $PLATFORM"
    echo "Supported platforms: macos, ios, ios-simulator"
    exit 1
fi

# Final verification
echo ""
echo "Verifying framework signature..."
codesign --verify --verbose --deep --strict "$FRAMEWORK_PATH"

echo ""
echo "=== Code Signing Complete ==="
