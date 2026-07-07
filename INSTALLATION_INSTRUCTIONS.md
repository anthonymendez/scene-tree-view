# Installation Instructions - Scene Tree View Plugin

This document provides detailed installation instructions for the Scene Tree View plugin across all supported platforms.

## Table of Contents

- [Windows Installation](#windows-installation)
- [Linux Installation](#linux-installation)
- [macOS Installation](#macos-installation)
- [Troubleshooting](#troubleshooting)

---

## Windows Installation

### Prerequisites
- Windows 10/11 (64-bit)
- OBS Studio 32.x+ installed

### Installation Steps

1. **Download the Plugin**
   - Download `obs-scene-tree-view-windows-x64.zip` from the [GitHub Releases](https://github.com/anthonymendez/scene-tree-view/releases) page

2. **Close OBS Studio**
   - Ensure OBS Studio is completely closed (check Task Manager if needed)

3. **Extract the Archive**
   - Extract the ZIP file to a temporary location

4. **Copy Plugin Files**
   - Copy the contents to `C:\Program Files\obs-studio\`
   - This will place:
     - `obs_scene_tree_view.dll` → `C:\Program Files\obs-studio\obs-plugins\64bit\`
     - `obs_scene_tree_view.pdb` → `C:\Program Files\obs-studio\obs-plugins\64bit\`
     - Locale files → `C:\Program Files\obs-studio\data\obs-plugins\obs_scene_tree_view\locale\`
   
   **PowerShell Example (Run as Administrator):**
   ```powershell
   $dest = "C:\Program Files\obs-studio\obs-plugins\64bit"
   Copy-Item ".\obs_scene_tree_view.dll" $dest -Force
   Copy-Item ".\obs_scene_tree_view.pdb" $dest -Force
   ```

5. **Launch OBS Studio**
   - Start OBS Studio

6. **Enable the Dock**
   - Go to **View → Docks → Scene Tree View** (check it)
   - If it doesn't appear: **View → Docks → Reset UI**, then re-check the dock

### Verification
- The Scene Tree View dock should appear (typically on the left side)
- Check OBS logs (Help → Log Files) for confirmation: Look for `[SceneTreeView] registered via add_custom_qdock`

---

## Linux Installation

### Prerequisites
- Linux distribution (Ubuntu 24.04, Fedora, Arch, etc.)
- OBS Studio 32.x+ installed
- Root/sudo access

### Installation Steps

1. **Download the Plugin**
   - Download `obs-scene-tree-view-linux-x86_64.zip` from the [GitHub Releases](https://github.com/anthonymendez/scene-tree-view/releases) page

2. **Close OBS Studio**
   - Ensure OBS Studio is completely closed

3. **Extract the Archive**
   ```bash
   unzip obs-scene-tree-view-linux-x86_64.zip
   cd obs-scene-tree-view-linux-x86_64
   ```

4. **Install Plugin Files (System-Level)**
   ```bash
   sudo cp -r usr/lib/obs-plugins/* /usr/lib/obs-plugins/
   sudo cp -r usr/share/obs/* /usr/share/obs/
   ```

5. **Launch OBS Studio**
   ```bash
   obs
   ```

6. **Enable the Dock**
   - Go to **View → Docks → Scene Tree View** (check it)
   - If it doesn't appear: **View → Docks → Reset UI**, then re-check the dock

### Important Notes
- **Version Matching**: Linux requires strict OBS version matching. The plugin must be built against the same libobs version as your OBS installation
- **System-Level Install**: This is a system-level install and requires root privileges
- **OBS 32.x Required**: Ensure you have OBS Studio 32.x or later

### Verification
- The Scene Tree View dock should appear
- Check OBS logs for confirmation

---

## macOS Installation

### Prerequisites
- macOS 13.0+ (Ventura or later)
- OBS Studio 32.x+ installed
- Administrator access

### Installation Steps

1. **Download the Plugin**
   - Download `obs-scene-tree-view-macos.zip` from the [GitHub Releases](https://github.com/anthonymendez/scene-tree-view/releases) page

2. **Close OBS Studio**
   - Ensure OBS Studio is completely closed

3. **Extract the Archive**
   - Double-click the ZIP file to extract it

4. **Install Plugin Files**
   - Copy the "Library" folder to the root of your disk (/) and allow merge
   - Or manually copy to:
     - `/Library/Application Support/obs-studio/plugins/obs_scene_tree_view.plugin/Contents/MacOS/obs_scene_tree_view`
     - `/Library/Application Support/obs-studio/plugins/obs_scene_tree_view/locale/*.ini`

5. **CRITICAL: Bypass macOS Gatekeeper**
   
   ⚠️ **This plugin is NOT code-signed. macOS Gatekeeper will block it on first launch.**
   
   You MUST bypass Gatekeeper using ONE of these methods:
   
   **Method 1 (Recommended - Right-Click):**
   1. Open Finder and navigate to `/Library/Application Support/obs-studio/plugins/obs_scene_tree_view.plugin`
   2. Right-click the plugin file
   3. Select "Open"
   4. Click "Open" in the security dialog
   5. The plugin will now work permanently
   
   **Method 2 (Terminal - xattr):**
   ```bash
   xattr -cr "/Library/Application Support/obs-studio/plugins/obs_scene_tree_view.plugin"
   ```
   Then restart OBS Studio.
   
   **Method 3 (System Settings):**
   1. Try to launch OBS with the plugin
   2. Open **System Settings → Privacy & Security**
   3. Scroll to the "Security" section
   4. Click "Open Anyway" next to the blocked plugin warning
   5. Restart OBS Studio

6. **Launch OBS Studio**
   - Start OBS Studio

7. **Enable the Dock**
   - Go to **View → Docks → Scene Tree View** (check it)
   - If it doesn't appear: **View → Docks → Reset UI**, then re-check the dock

### Important Notes
- **Universal Binary**: This plugin supports both Intel (x86_64) and Apple Silicon (arm64) Macs
- **Unsigned Binary**: The plugin is NOT code-signed. Gatekeeper bypass is required on first launch
- **System-Level Install**: May require administrator privileges
- **OBS 32.x Required**: Ensure you have OBS Studio 32.x or later

### Verification
- The Scene Tree View dock should appear
- Check OBS logs (Help → Log Files) for confirmation

---

## Troubleshooting

### Plugin Not Appearing in OBS (All Platforms)

**Problem**: The Scene Tree View dock doesn't appear in the Docks menu.

**Solutions**:
1. Verify the plugin files are installed in the correct location (see platform-specific sections above)
2. In OBS, enable the dock: **View → Docks → Scene Tree View** (check it)
3. If missing: **View → Docks → Reset UI**, then re-check the dock entry
4. Check OBS logs for clues (Help → Log Files): Look for lines containing `obs_scene_tree_view` and `registered via`
5. Ensure OBS Studio is 32.x+ (Help → About OBS Studio)

### macOS Gatekeeper Issues

**Problem**: macOS blocks the plugin with "cannot be opened because the developer cannot be verified"

**Solution**: Follow the Gatekeeper bypass instructions in the macOS Installation section above (Method 1, 2, or 3)

### Linux Version Mismatch

**Problem**: Plugin fails to load on Linux

**Solution**: Linux requires strict version matching. Ensure the plugin was built against the same libobs version as your OBS installation. You may need to build from source if the pre-built binary doesn't match your OBS version.

### Windows Permission Issues

**Problem**: Cannot copy files to `C:\Program Files\obs-studio\`

**Solution**: Run PowerShell or Command Prompt as Administrator, or use File Explorer with administrator privileges

---

## Support

For additional help:
1. Check the main [README.md](README.md) for more information
2. Search existing [GitHub Issues](https://github.com/anthonymendez/scene-tree-view/issues)
3. Create a new issue with:
   - OBS Studio version
   - Plugin version
   - Operating system and version
   - Detailed description of the problem
   - OBS log file (Help → Log Files)

