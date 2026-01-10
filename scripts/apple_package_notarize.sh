#!/bin/bash
# =============================================================================
# Apple Package and Notarize Script for LSL
# =============================================================================
# Creates a macOS installer package (.pkg) and optionally notarizes it.
#
# Usage:
#   ./scripts/apple_package_notarize.sh <framework_path> [--notarize] [--output <dir>]
#
# Environment Variables:
#   APPLE_CODE_SIGN_IDENTITY_INST - Installer signing identity (default: "Developer ID Installer")
#   APPLE_DEVELOPMENT_TEAM        - Team ID for notarization (required for --notarize)
#   APPLE_NOTARIZE_USERNAME       - Apple ID for notarization (required for --notarize)
#   APPLE_NOTARIZE_PASSWORD       - App-specific password (required for --notarize)
#
# Examples:
#   # Create signed installer package (no notarization)
#   ./scripts/apple_package_notarize.sh install/Frameworks/lsl.framework
#
#   # Create and notarize installer package
#   ./scripts/apple_package_notarize.sh install/Frameworks/lsl.framework --notarize
#
#   # Specify output directory
#   ./scripts/apple_package_notarize.sh install/Frameworks/lsl.framework --output package/
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Default configuration
SIGN_IDENTITY="${APPLE_CODE_SIGN_IDENTITY_INST:-Developer ID Installer}"
TEAM_ID="${APPLE_DEVELOPMENT_TEAM:-}"
NOTARIZE_USERNAME="${APPLE_NOTARIZE_USERNAME:-}"
NOTARIZE_PASSWORD="${APPLE_NOTARIZE_PASSWORD:-}"
OUTPUT_DIR="."
DO_NOTARIZE=false

# Parse arguments
FRAMEWORK_PATH=""
while [[ $# -gt 0 ]]; do
    case $1 in
        --notarize)
            DO_NOTARIZE=true
            shift
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
            echo "Usage: $0 <framework_path> [--notarize] [--output <dir>]"
            echo ""
            echo "Options:"
            echo "  --notarize     Submit to Apple for notarization and staple the ticket"
            echo "  --output <dir> Output directory for the .pkg file (default: current directory)"
            echo "  --identity     Override installer signing identity"
            echo ""
            echo "Environment Variables:"
            echo "  APPLE_CODE_SIGN_IDENTITY_INST - Installer signing identity"
            echo "  APPLE_DEVELOPMENT_TEAM        - Team ID for notarization"
            echo "  APPLE_NOTARIZE_USERNAME       - Apple ID for notarization"
            echo "  APPLE_NOTARIZE_PASSWORD       - App-specific password for notarization"
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
    echo "Usage: $0 <framework_path> [--notarize] [--output <dir>]"
    exit 1
fi

if [[ ! -d "$FRAMEWORK_PATH" ]]; then
    echo "Error: Framework not found at: $FRAMEWORK_PATH"
    exit 1
fi

# Get version from framework's Info.plist
INFO_PLIST="$FRAMEWORK_PATH/Versions/A/Resources/Info.plist"
if [[ ! -f "$INFO_PLIST" ]]; then
    # Try alternative location
    INFO_PLIST="$FRAMEWORK_PATH/Resources/Info.plist"
fi

if [[ -f "$INFO_PLIST" ]]; then
    LSL_VERSION=$(/usr/libexec/PlistBuddy -c "Print CFBundleShortVersionString" "$INFO_PLIST")
else
    echo "Warning: Could not find Info.plist, using 'unknown' as version"
    LSL_VERSION="unknown"
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Package filename
PKG_NAME="liblsl-${LSL_VERSION}-Darwin-universal.pkg"
PKG_PATH="$OUTPUT_DIR/$PKG_NAME"

echo "=== Apple Package Creation ==="
echo "Framework: $FRAMEWORK_PATH"
echo "Version: $LSL_VERSION"
echo "Output: $PKG_PATH"
echo "Identity: $SIGN_IDENTITY"
echo ""

# Create the installer package
echo "Creating installer package..."
productbuild --sign "$SIGN_IDENTITY" \
    --component "$FRAMEWORK_PATH" \
    /Library/Frameworks \
    "$PKG_PATH"

echo "Package created: $PKG_PATH"

# Notarization
if [[ "$DO_NOTARIZE" == true ]]; then
    echo ""
    echo "=== Notarization ==="

    # Check required environment variables
    if [[ -z "$NOTARIZE_USERNAME" ]]; then
        echo "Error: APPLE_NOTARIZE_USERNAME not set"
        exit 1
    fi
    if [[ -z "$NOTARIZE_PASSWORD" ]]; then
        echo "Error: APPLE_NOTARIZE_PASSWORD not set"
        exit 1
    fi
    if [[ -z "$TEAM_ID" ]]; then
        echo "Error: APPLE_DEVELOPMENT_TEAM not set"
        exit 1
    fi

    echo "Submitting to Apple notarization service..."
    echo "  Apple ID: $NOTARIZE_USERNAME"
    echo "  Team ID: $TEAM_ID"
    echo ""

    xcrun notarytool submit "$PKG_PATH" \
        --apple-id "$NOTARIZE_USERNAME" \
        --password "$NOTARIZE_PASSWORD" \
        --team-id "$TEAM_ID" \
        --wait

    echo ""
    echo "Stapling notarization ticket..."
    xcrun stapler staple "$PKG_PATH"

    echo ""
    echo "Validating stapled ticket..."
    xcrun stapler validate "$PKG_PATH"
fi

echo ""
echo "=== Package Complete ==="
echo "Output: $PKG_PATH"

# Export for use in CI
if [[ -n "$GITHUB_ENV" ]]; then
    echo "LSL_VERSION=$LSL_VERSION" >> "$GITHUB_ENV"
    echo "PKG_PATH=$PKG_PATH" >> "$GITHUB_ENV"
fi
