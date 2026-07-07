#!/usr/bin/env bash

# macOS installation script for Scene Tree View OBS Plugin
# Installs to user-level plugins folder to avoid requiring root privileges.

set -euo pipefail

# Define colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}====================================================${NC}"
echo -e "${BLUE}  Scene Tree View - macOS Installer                 ${NC}"
echo -e "${BLUE}====================================================${NC}"

# Check dependencies
if ! command -v curl >/dev/null 2>&1; then
    echo -e "${RED}Error: curl is required to run this installer.${NC}" >&2
    exit 1
fi

if ! command -v unzip >/dev/null 2>&1; then
    echo -e "${RED}Error: unzip is required to extract plugin files.${NC}" >&2
    exit 1
fi

# Check if OBS Studio is running
if pgrep -x "obs" >/dev/null || pgrep -x "obs-studio" >/dev/null || pgrep -f "bin/obs" >/dev/null; then
    echo -e "${YELLOW}WARNING: OBS Studio is currently running!${NC}"
    read -p "Do you want to close OBS now? (y/n): " close_obs
    if [[ "$close_obs" =~ ^[yY]$ ]]; then
        echo "Closing OBS Studio..."
        pkill -x "obs" || pkill -x "obs-studio" || pkill -f "bin/obs"
        sleep 2
    else
        echo -e "${RED}Installation cancelled. Please close OBS Studio and run the script again.${NC}"
        exit 1
    fi
fi

# Fetch latest release info from GitHub API
echo "Fetching latest release information from GitHub..."
LATEST_RELEASE=$(curl -fsSL https://api.github.com/repos/anthonymendez/scene-tree-view/releases/latest)

# Extract download URL for macOS asset
ZIP_URL=$(echo "$LATEST_RELEASE" | grep "browser_download_url" | cut -d '"' -f 4 | grep "macos.zip" | head -n 1)

if [ -z "$ZIP_URL" ]; then
    echo -e "${RED}Error: Could not find the macOS zip asset in the latest release.${NC}" >&2
    exit 1
fi

# Create a temporary directory
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

echo -e "Downloading latest release: ${YELLOW}$(basename "$ZIP_URL")${NC}..."
curl -L -o "$TEMP_DIR/plugin.zip" "$ZIP_URL"

echo "Extracting files..."
unzip -q "$TEMP_DIR/plugin.zip" -d "$TEMP_DIR/extracted"

# Target user plugins folder on macOS
TARGET_DIR="$HOME/Library/Application Support/obs-studio/plugins"
echo -e "Installing to: ${YELLOW}$TARGET_DIR${NC}..."

# Ensure target directory exists
mkdir -p "$TARGET_DIR"

# Copy plugin files
# The source structure inside the zip starts with "obs-scene-tree-view-macos/Library/Application Support/obs-studio/plugins/"
SRC_PLUGINS_DIR=$(find "$TEMP_DIR/extracted" -path "*/Library/Application Support/obs-studio/plugins" -type d | head -n 1)

if [ -n "$SRC_PLUGINS_DIR" ] && [ -d "$SRC_PLUGINS_DIR" ]; then
    # Copy files recursively (handles both .plugin bundle and the data/locale files)
    cp -R "$SRC_PLUGINS_DIR/"* "$TARGET_DIR/"
else
    echo -e "${RED}Error: Plugin directory structure not found in the downloaded archive.${NC}" >&2
    exit 1
fi

# Bypass Gatekeeper by removing quarantine attributes
PLUGIN_PATH="$TARGET_DIR/obs_scene_tree_view.plugin"
if [ -d "$PLUGIN_PATH" ]; then
    echo "Bypassing macOS Gatekeeper (removing quarantine attributes)..."
    xattr -cr "$PLUGIN_PATH"
else
    echo -e "${YELLOW}Warning: Could not find installed .plugin bundle at $PLUGIN_PATH to run xattr.${NC}"
fi

echo -e "\n${GREEN}✔ Installation completed successfully!${NC}"
echo -e "Next steps:"
echo -e "  1. Start/Restart OBS Studio."
echo -e "  2. Go to ${YELLOW}View → Docks → Scene Tree View${NC} to check and enable the dock."
echo -e "  3. If it doesn't show up, try ${YELLOW}View → Docks → Reset UI${NC}."
