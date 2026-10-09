# Scene Tree View — AI Agent Guide

Shared instructions for AI coding agents (Claude Code, Antigravity/Gemini, etc.). `CLAUDE.md` and `GEMINI.md` import this file — edit here, not there.

Scene Tree View is an OBS Studio plugin written in C++ with Qt 6, built with CMake.

## Coding style
Follow the Google C++ Style Guide, kept locally at [.gemini/google_cpp_style_guide.md](.gemini/google_cpp_style_guide.md). Read the relevant section when unsure rather than loading the whole file.

## Developer workflow
After C++ changes or when new authors have committed:
1. Update credits:
   ```bash
   ./scripts/update-authors.sh
   ```
2. Compile and install:
   ```bash
   ./scripts/build-linux.sh
   ```
   Choose `1` to install locally to the OBS config folder.

---

## Build & installation script (`scripts/build-linux.sh`)
Builds and installs on Arch Linux and derivatives (e.g. CachyOS). See [scripts/build-linux.sh](scripts/build-linux.sh).

- **Dependencies**: checks for and installs `obs-studio`, `cmake`, `ninja`, and `qt6-base` via `pacman`.
- **Environment**: forwards `OBS_SDK_DIR` to CMake if set.
- **NTFS safe-linking**: switches the linker to `lld` when the build directory is on NTFS (see below).
- **Post-build menu**:
  1. **Local user install (recommended)**: installs the `.so` and locale files to `~/.config/obs-studio/plugins/obs_scene_tree_view/`, no root needed.
  2. **System-wide install**: `sudo cmake --install build`, deploying to `/usr/lib/obs-plugins` and `/usr/share/obs/obs-plugins/`.
  3. **Package only**: builds a distributable ZIP in `dist/`.

## Known issue: GNU BFD linker crashes on NTFS mounts
Building on NTFS-3G or `ntfs3` partitions on Linux makes the default GNU BFD linker (`ld`) crash:
```text
/usr/bin/ld: BFD (GNU Binutils) internal error, aborting at ... in elf_mmap_section_contents
collect2: error: ld returned 1 exit status
```
BFD fails trying to `mmap` output sections onto NTFS. The build script detects `ntfs`/`fuse` via `df -T` and adds `-DCMAKE_SHARED_LINKER_FLAGS="-fuse-ld=lld"` to link with LLVM `lld` instead. Do not remove this workaround.

## Tools menu & About dialog
- Registered on `OBS_FRONTEND_EVENT_FINISHED_LOADING`, appending an action to the OBS **Tools** menu under the localized plugin name (`obs_module_name()`).
- Opens a `QMessageBox` parented to the OBS main window.
- The dialog body is HTML and uses `mb.setTextInteractionFlags(Qt::TextBrowserInteraction)` so the GitHub link is clickable.
- The `SceneTreeView` translation key is `"Scene Tree View"` in `en-US.ini`, `pt-BR.ini`, and `ru-RU.ini`; keep locale files in sync when changing strings.

## Contributor credits (`scripts/update-authors.sh`)
- Git identity for the maintainer: `Anthony Mendez <anthonymendez9@gmail.com>`.
- [scripts/update-authors.sh](scripts/update-authors.sh) extracts unique authors from `git log`, orders them by most recent commit, and writes the `"contributors"` field of `buildspec.json`.
- **`buildspec.json` is the single source of truth.** CMake parses it at configure time and generates `version.h` defines (`PROJECT_AUTHOR`, `PROJECT_CONTRIBUTORS`, `PROJECT_WEBSITE`, etc.). Never hardcode metadata in C++ source; edit `buildspec.json` instead.
