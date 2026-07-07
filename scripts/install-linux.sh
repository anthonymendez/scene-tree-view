#!/usr/bin/env bash

# Distro-agnostic installation script for Scene Tree View OBS Plugin
# Installs to user-level plugins folder to avoid requiring root privileges.

set -euo pipefail

# Define colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}====================================================${NC}"
echo -e "${BLUE}  Scene Tree View - Linux Installer                 ${NC}"
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

# Fetch the latest release info from GitHub API
echo "Fetching latest release information from GitHub..."
LATEST_RELEASE=$(curl -fsSL https://api.github.com/repos/anthonymendez/scene-tree-view/releases/latest)

# Extract download URL for the linux asset
ZIP_URL=$(echo "$LATEST_RELEASE" | grep "browser_download_url" | cut -d '"' -f 4 | grep "linux-x86_64.zip" | head -n 1)

if [ -z "$ZIP_URL" ]; then
    echo -e "${RED}Error: Could not find the Linux zip asset in the latest release.${NC}" >&2
    exit 1
fi

# Create a temporary directory
TEMP_DIR=$(mktemp -d)
trap 'rm -rf "$TEMP_DIR"' EXIT

echo -e "Downloading latest release: ${YELLOW}$(basename "$ZIP_URL")${NC}..."
curl -L -o "$TEMP_DIR/plugin.zip" "$ZIP_URL"

echo "Extracting files..."
unzip -q "$TEMP_DIR/plugin.zip" -d "$TEMP_DIR/extracted"

# Identify potential user-level OBS directories
PATHS=()
if [ -d "$HOME/.config/obs-studio" ]; then
    PATHS+=("$HOME/.config/obs-studio/plugins/obs_scene_tree_view")
fi
if [ -d "$HOME/.var/app/com.obsproject.Studio/config/obs-studio" ]; then
    PATHS+=("$HOME/.var/app/com.obsproject.Studio/config/obs-studio/plugins/obs_scene_tree_view")
fi
if [ -d "$HOME/snap/obs-studio/current/.config/obs-studio" ]; then
    PATHS+=("$HOME/snap/obs-studio/current/.config/obs-studio/plugins/obs_scene_tree_view")
fi

# If no directories were found, default to standard user path
if [ ${#PATHS[@]} -eq 0 ]; then
    PATHS+=("$HOME/.config/obs-studio/plugins/obs_scene_tree_view")
fi

# Install files to detected directories
INSTALLED_COUNT=0
for path in "${PATHS[@]}"; do
    echo -e "Installing to: ${YELLOW}$path${NC}..."
    
    mkdir -p "$path/bin/64bit"
    mkdir -p "$path/data/locale"

    # Copy files
    # The source structure inside zip:
    # obs-scene-tree-view-linux-x86_64/usr/lib/obs-plugins/obs_scene_tree_view.so
    # obs-scene-tree-view-linux-x86_64/usr/share/obs/obs-plugins/obs_scene_tree_view/locale/*.ini
    
    SO_FILE=$(find "$TEMP_DIR/extracted" -name "obs_scene_tree_view.so" | head -n 1)
    LOCALE_DIR=$(find "$TEMP_DIR/extracted" -path "*/locale" | head -n 1)

    if [ -n "$SO_FILE" ] && [ -f "$SO_FILE" ]; then
        cp "$SO_FILE" "$path/bin/64bit/"
    else
        echo -e "${RED}Error: Plugin library not found in the downloaded archive.${NC}" >&2
        exit 1
    fi

    if [ -n "$LOCALE_DIR" ] && [ -d "$LOCALE_DIR" ]; then
        cp "$LOCALE_DIR"/*.ini "$path/data/locale/"
    else
        echo -e "${YELLOW}Warning: Locale directory not found or empty in archive.${NC}"
    fi
    
    INSTALLED_COUNT=$((INSTALLED_COUNT + 1))
done

echo -e "\n${GREEN}✔ Installation completed successfully for $INSTALLED_COUNT configuration(s)!${NC}"
echo -e "Next steps:"
echo -e "  1. Start/Restart OBS Studio."
echo -e "  2. Go to ${YELLOW}View → Docks → Scene Tree View${NC} to check and enable the dock."
echo -e "  3. If it doesn't show up, try ${YELLOW}View → Docks → Reset UI${NC}."
