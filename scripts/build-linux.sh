#!/usr/bin/env bash

# Exit immediately if a command exits with a non-zero status
set -e

# Define colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}====================================================${NC}"
echo -e "${BLUE}  Scene Tree View Build & Install (Arch/CachyOS)    ${NC}"
echo -e "${BLUE}  Based on README.md instructions                   ${NC}"
echo -e "${BLUE}====================================================${NC}"

# Check for Arch Linux / CachyOS package manager (pacman)
if ! command -v pacman &> /dev/null; then
    echo -e "${RED}Error: pacman not found. This script is intended for CachyOS/Arch Linux systems.${NC}"
    exit 1
fi

# 1. Install dependencies (Arch Linux section of README.md)
echo -e "\n${BLUE}[1/4] Checking and installing dependencies...${NC}"
echo -e "Installing dependencies: ${YELLOW}obs-studio cmake ninja qt6-base${NC} via pacman"
sudo pacman -S --needed --noconfirm obs-studio cmake ninja qt6-base

# 2. Setup Environment Variables
echo -e "\n${BLUE}[2/4] Setting up environment...${NC}"
CMAKE_ARGS=("-S" "." "-B" "build" "-G" "Ninja" "-DCMAKE_BUILD_TYPE=Release" "-DCMAKE_INSTALL_PREFIX=/usr")

if [ -n "${OBS_SDK_DIR:-}" ]; then
    echo -e "Using OBS_SDK_DIR: ${YELLOW}${OBS_SDK_DIR}${NC}"
    CMAKE_ARGS+=("-DOBS_SDK_DIR=${OBS_SDK_DIR}")
else
    echo -e "OBS_SDK_DIR is not set. Using system-wide OBS headers."
fi

# Detect NTFS to prevent GNU BFD linker crash
if df -T . | grep -E -q "ntfs|fuse"; then
    if command -v ld.lld &> /dev/null || which ld.lld &> /dev/null; then
        echo -e "${YELLOW}NTFS mount detected. Linking with ld.lld to avoid GNU BFD linker crashes on NTFS filesystem.${NC}"
        CMAKE_ARGS+=("-DCMAKE_SHARED_LINKER_FLAGS=-fuse-ld=lld")
    fi
fi

# 3. Configure and Build (Linux section of README.md)
echo -e "\n${BLUE}[3/4] Configuring with CMake...${NC}"
cmake "${CMAKE_ARGS[@]}"

echo -e "\n${BLUE}[4/4] Building project...${NC}"
cmake --build build --config Release

echo -e "\n${GREEN}✔ Build completed successfully!${NC}"
echo -e "Built plugin binary path: ${YELLOW}build/obs_scene_tree_view.so${NC}"

# 4. Installation / Packaging Options (based on README.md / package scripts)
echo -e "\n${BLUE}Choose installation option:${NC}"
echo -e "1) ${GREEN}Local User Install${NC} (Recommended - Installs to ~/.config/obs-studio/plugins/)"
echo -e "2) ${YELLOW}System-wide Install${NC} (Matches README: 'sudo cmake --install build')"
echo -e "3) ${BLUE}Package Only${NC} (Create a distributable zip archive under dist/)"
echo -e "4) ${RED}Do Not Install / Exit${NC}"

read -p "Enter choice [1-4]: " choice

LOCAL_PLUGIN_DIR="$HOME/.config/obs-studio/plugins/obs_scene_tree_view"

case $choice in
    1)
        echo -e "\n${BLUE}Installing locally to ${LOCAL_PLUGIN_DIR}...${NC}"
        mkdir -p "${LOCAL_PLUGIN_DIR}/bin/64bit"
        mkdir -p "${LOCAL_PLUGIN_DIR}/data/locale"
        
        cp "build/obs_scene_tree_view.so" "${LOCAL_PLUGIN_DIR}/bin/64bit/"
        cp -v data/locale/*.ini "${LOCAL_PLUGIN_DIR}/data/locale/"
        
        echo -e "\n${GREEN}✔ Installed successfully!${NC}"
        echo -e "Please restart OBS Studio to load the plugin."
        echo -e "Enable the dock under: ${YELLOW}View → Docks → Scene Tree View${NC}"
        ;;
    2)
        echo -e "\n${BLUE}Installing system-wide (requires root)...${NC}"
        sudo cmake --install build --config Release
        
        echo -e "\n${GREEN}✔ Installed successfully system-wide!${NC}"
        echo -e "Please restart OBS Studio to load the plugin."
        echo -e "Enable the dock under: ${YELLOW}View → Docks → Scene Tree View${NC}"
        ;;
    3)
        echo -e "\n${BLUE}Packaging the build...${NC}"
        if [ -f "scripts/package-linux.sh" ]; then
            bash scripts/package-linux.sh build dist
        else
            echo -e "${RED}Error: scripts/package-linux.sh not found.${NC}"
            exit 1
        fi
        ;;
    *)
        echo -e "\n${YELLOW}Exiting without installing.${NC}"
        ;;
esac
