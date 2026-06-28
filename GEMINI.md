# Gemini AI Development Log - Scene Tree View

This document logs the development, debugging, and feature additions made during the development session. It serves as a reference for developers working on this project, highlighting solutions to specific platform issues (such as NTFS compilation bugs) and the automation tools created.

---

## 1. Automated Build & Installation (`scripts/build-linux.sh`)
To simplify building and installing on Arch Linux and its derivatives (such as CachyOS), a robust C++ build script was created under [scripts/build-linux.sh](file:///run/media/anthony/Roommate/Projects/scene-tree-view/scripts/build-linux.sh).

### Features:
- **Dependency Management**: Automatically checks for and installs missing dependencies (`obs-studio`, `cmake`, `ninja`, and `qt6-base`) using `pacman`.
- **Environment Detection**: Detects if `OBS_SDK_DIR` is set in the environment and forwards it to CMake.
- **NTFS Safe-Linking (Crucial Fix)**: Detects if the build directory is on a mounted NTFS filesystem and switches the linker to `lld` (see section below).
- **Interactive Post-Build Menu**:
  1. **Local User Install (Recommended)**: Installs the `.so` and locale files to `~/.config/obs-studio/plugins/obs_scene_tree_view/`, allowing the plugin to run without modifying system directories or requiring root permissions.
  2. **System-wide Install**: Runs `sudo cmake --install build` to deploy files to `/usr/lib/obs-plugins` and `/usr/share/obs/obs-plugins/`.
  3. **Package Only**: Builds a distributable ZIP bundle in `dist/`.

---

## 2. Solved: GNU BFD Linker Crashes on NTFS mounts
### The Issue:
When compiling the plugin on directories hosted on NTFS-3G or `ntfs3` mounted partitions on Linux, the default GNU BFD linker (`ld`) crashes with an internal alignment / memory mapping error:
```text
/usr/bin/ld: BFD (GNU Binutils) internal error, aborting at ... in elf_mmap_section_contents
collect2: error: ld returned 1 exit status
```
This is a known bug with BFD trying to `mmap` output sections directly onto NTFS filesystems on Linux.

### The Solution:
The build script checks the partition type using `df -T`. If it detects `ntfs` or `fuse`, it automatically appends `-DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld"` to switch from the default BFD linker to LLVM's **`lld`** linker (`ld.lld`), which bypasses the memory mapping issues and links the shared library successfully.

---

## 3. Tools Menu & QMessageBox "About" Dialog
A new dropdown menu item was registered under the main OBS Studio **Tools** menu.

### Features:
- **Tools Integration**: Fired during `OBS_FRONTEND_EVENT_FINISHED_LOADING` to dynamically append the action to the Tools menu under the localized plugin name (`obs_module_name()`).
- **Interactive About Box**: Spawns a Qt `QMessageBox` using the parent OBS main window context.
- **Clickable Hyperlinks**: The dialog is formatted with HTML and uses `mb.setTextInteractionFlags(Qt::TextBrowserInteraction)` to ensure the GitHub repository link is selectable and clickable.
- **Dynamic Localization**: Updates the `SceneTreeView` translation key to `"Scene Tree View"` in `en-US.ini`, `pt-BR.ini`, and `ru-RU.ini` locale files so that the menu entry and window title reflect correct translation strings.

---

## 4. Git Author Alignment & Dynamic Contributor Credits (`scripts/update-authors.sh`)
To keep the credits inside the plugin's metadata and the About dialog synchronized with the actual repository history, we:
1. **Standardized Git Identity**: Aligned local commits to use your full name: `Anthony Mendez <anthonymendez9@gmail.com>`.
2. **Created Automation Utility**: Wrote [scripts/update-authors.sh](file:///run/media/anthony/Roommate/Projects/scene-tree-view/scripts/update-authors.sh).

### How the Author Script Works:
- Runs a Python parsing script that extracts all unique commit authors from `git log`.
- Orders the authors based on the timestamp of their **latest commit** (most recently active author listed first).
- Automates editing of **`buildspec.json`** instead of editing source code directly, updating the `"contributors"` field.
- **Single Source of Truth**: CMake reads and parses `buildspec.json` during the build configuration, generating preprocessor definitions in `version.h` (`PROJECT_AUTHOR`, `PROJECT_CONTRIBUTORS`, `PROJECT_WEBSITE`, etc.). This keeps the C++ source files completely static and decoupled from metadata changes.

---

## Developer Workflow Recap
When you make changes to the C++ code or when new authors submit commits to the repo:
1. **Update Credits**:
   ```bash
   ./scripts/update-authors.sh
   ```
2. **Compile and Install**:
   ```bash
   ./scripts/build-linux.sh
   ```
   *(Select `1` to install locally to OBS config folder)*

