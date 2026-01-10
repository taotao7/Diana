#!/bin/bash
set -euo pipefail

# =============================================================================
# Diana DMG Packaging Script
# Creates a .app bundle and packages it into a DMG
# =============================================================================

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"
PACKAGING_DIR="${PROJECT_ROOT}/packaging"

# App configuration
APP_NAME="Diana"
BUNDLE_ID="com.diana.app"
VERSION="${VERSION:-0.1.0}"

# Paths
APP_BUNDLE="${BUILD_DIR}/${APP_NAME}.app"
DMG_NAME="${APP_NAME}-${VERSION}-$(uname -m).dmg"
DMG_PATH="${BUILD_DIR}/${DMG_NAME}"

# =============================================================================
# Functions
# =============================================================================

log() {
    echo "[$(date '+%H:%M:%S')] $*"
}

generate_icns() {
    log "Generating AppIcon.icns from resources/icon.png..."
    
    local source_icon="${PROJECT_ROOT}/resources/icon.png"
    local iconset_dir="${BUILD_DIR}/AppIcon.iconset"
    local icns_output="${BUILD_DIR}/AppIcon.icns"
    
    if [[ ! -f "$source_icon" ]]; then
        log "Warning: Source icon not found at ${source_icon}"
        return 1
    fi
    
    # Clean previous iconset
    rm -rf "$iconset_dir"
    mkdir -p "$iconset_dir"
    
    # Generate all required icon sizes using sips
    # macOS .icns requires specific sizes: 16, 32, 128, 256, 512 (and @2x variants)
    local sizes=(16 32 128 256 512)
    
    for size in "${sizes[@]}"; do
        # Standard resolution
        sips -z "$size" "$size" "$source_icon" --out "${iconset_dir}/icon_${size}x${size}.png" >/dev/null 2>&1
        
        # @2x (Retina) resolution
        local size_2x=$((size * 2))
        if [[ $size_2x -le 1024 ]]; then
            sips -z "$size_2x" "$size_2x" "$source_icon" --out "${iconset_dir}/icon_${size}x${size}@2x.png" >/dev/null 2>&1
        fi
    done
    
    # Convert iconset to icns using iconutil
    iconutil -c icns "$iconset_dir" -o "$icns_output"
    
    # Clean up iconset directory
    rm -rf "$iconset_dir"
    
    if [[ -f "$icns_output" ]]; then
        log "Generated: $icns_output"
        return 0
    else
        log "Warning: Failed to generate icns"
        return 1
    fi
}

error() {
    echo "[ERROR] $*" >&2
    exit 1
}

check_binary() {
    local binary="${BUILD_DIR}/diana"
    if [[ ! -f "$binary" ]]; then
        error "diana binary not found at ${binary}. Run cmake --build first."
    fi
    log "Found binary: $binary"
}

create_app_bundle() {
    log "Creating .app bundle structure..."
    
    # Clean previous bundle
    rm -rf "$APP_BUNDLE"
    
    # Create directory structure
    mkdir -p "${APP_BUNDLE}/Contents/MacOS"
    mkdir -p "${APP_BUNDLE}/Contents/Resources"
    
    # Copy executable
    cp "${BUILD_DIR}/diana" "${APP_BUNDLE}/Contents/MacOS/"
    chmod +x "${APP_BUNDLE}/Contents/MacOS/diana"
    
    # Copy Info.plist with version substitution
    sed "s/@PROJECT_VERSION@/${VERSION}/g" \
        "${PACKAGING_DIR}/Info.plist.in" > "${APP_BUNDLE}/Contents/Info.plist"
    
    # Copy resources (fonts)
    if [[ -d "${BUILD_DIR}/resources" ]]; then
        cp -R "${BUILD_DIR}/resources" "${APP_BUNDLE}/Contents/Resources/"
        log "Copied resources"
    fi
    
    # Copy generated icon
    if [[ -f "${BUILD_DIR}/AppIcon.icns" ]]; then
        cp "${BUILD_DIR}/AppIcon.icns" "${APP_BUNDLE}/Contents/Resources/"
        log "Copied app icon"
    else
        log "Warning: No AppIcon.icns found in ${BUILD_DIR}"
    fi
    
    # Create PkgInfo
    echo -n "APPL????" > "${APP_BUNDLE}/Contents/PkgInfo"
    
    log "App bundle created: $APP_BUNDLE"
}

fix_library_paths() {
    log "Fixing dynamic library paths..."
    
    local executable="${APP_BUNDLE}/Contents/MacOS/diana"
    local frameworks_dir="${APP_BUNDLE}/Contents/Frameworks"
    
    # Get list of non-system dylibs
    local dylibs
    dylibs=$(otool -L "$executable" | grep -v '/System\|/usr/lib\|@executable\|@rpath' | awk 'NR>1 {print $1}' || true)
    
    if [[ -n "$dylibs" ]]; then
        mkdir -p "$frameworks_dir"
        
        while IFS= read -r dylib; do
            if [[ -f "$dylib" ]]; then
                local dylib_name
                dylib_name=$(basename "$dylib")
                
                # Copy dylib
                cp "$dylib" "${frameworks_dir}/${dylib_name}"
                
                # Update reference
                install_name_tool -change "$dylib" \
                    "@executable_path/../Frameworks/${dylib_name}" \
                    "$executable"
                
                log "Bundled: $dylib_name"
            fi
        done <<< "$dylibs"
    else
        log "No external dylibs to bundle (static linking)"
    fi
}

codesign_app() {
    local identity="${CODESIGN_IDENTITY:-}"
    
    if [[ -n "$identity" ]]; then
        log "Code signing with identity: $identity"
        codesign --force --deep --sign "$identity" \
            --options runtime \
            --entitlements "${PACKAGING_DIR}/entitlements.plist" \
            "$APP_BUNDLE" 2>/dev/null || {
            log "Warning: Code signing failed (continuing without signature)"
        }
    else
        log "Skipping code signing (CODESIGN_IDENTITY not set)"
        # Ad-hoc signing for local testing
        codesign --force --deep --sign - "$APP_BUNDLE" 2>/dev/null || true
    fi
}

create_dmg() {
    log "Creating DMG..."
    
    # Clean previous DMG
    rm -f "$DMG_PATH"
    
    local temp_dmg="${BUILD_DIR}/temp_${APP_NAME}.dmg"
    local mount_point="${BUILD_DIR}/dmg_mount"
    
    # Calculate size (app size + 20MB buffer)
    local app_size
    app_size=$(du -sm "$APP_BUNDLE" | cut -f1)
    local dmg_size=$((app_size + 20))
    
    # Create temporary DMG
    hdiutil create -size "${dmg_size}m" -fs HFS+ -volname "$APP_NAME" \
        "$temp_dmg" -quiet
    
    # Mount
    hdiutil attach "$temp_dmg" -mountpoint "$mount_point" -quiet
    
    # Copy app
    cp -R "$APP_BUNDLE" "${mount_point}/"
    
    # Create Applications symlink
    ln -s /Applications "${mount_point}/Applications"
    
    # Set background and icon positions (optional, basic layout)
    # For advanced DMG styling, use create-dmg or dmgbuild tools
    
    # Unmount
    hdiutil detach "$mount_point" -quiet
    
    # Convert to compressed DMG
    hdiutil convert "$temp_dmg" -format UDZO -imagekey zlib-level=9 \
        -o "$DMG_PATH" -quiet
    
    # Clean up
    rm -f "$temp_dmg"
    
    log "DMG created: $DMG_PATH"
    
    # Show DMG info
    local dmg_size_mb
    dmg_size_mb=$(du -h "$DMG_PATH" | cut -f1)
    log "DMG size: $dmg_size_mb"
}

notarize_dmg() {
    local apple_id="${APPLE_ID:-}"
    local team_id="${APPLE_TEAM_ID:-}"
    local password="${APPLE_APP_PASSWORD:-}"
    
    if [[ -n "$apple_id" && -n "$team_id" && -n "$password" ]]; then
        log "Submitting for notarization..."
        
        xcrun notarytool submit "$DMG_PATH" \
            --apple-id "$apple_id" \
            --team-id "$team_id" \
            --password "$password" \
            --wait
        
        # Staple
        xcrun stapler staple "$DMG_PATH"
        
        log "Notarization complete"
    else
        log "Skipping notarization (APPLE_ID, APPLE_TEAM_ID, APPLE_APP_PASSWORD not set)"
    fi
}

# =============================================================================
# Main
# =============================================================================

main() {
    log "Starting Diana packaging (version: $VERSION)"
    
    check_binary
    generate_icns
    create_app_bundle
    fix_library_paths
    codesign_app
    create_dmg
    notarize_dmg
    
    log "Packaging complete!"
    log "Output: $DMG_PATH"
}

main "$@"
