# Version History - Scene Tree View

This document tracks the main updates, fixes, and changes made to the Scene Tree View plugin across different versions, parsed from the git history.

---

### v0.2.4
* **Author**: Anthony Mendez
- **Multi-Selection & Group Move**: Added support for selecting multiple scenes/folders (`Ctrl`/`Shift` multi-selection) to drag-and-drop or move them as a group.
- **MIME Serialization Fix**: Fixed memory offset reading bug when processing multiple items in `dropMimeData`.
- **Folder Sorting Fix**: Ensured newly created and moved folders preserve the sorting state of their parent/original container.

### v0.2.3
* **Author**: Anthony Mendez
- **Alphabetical Sorting**: Added folder-level alphabetical sorting support with 3 separate modes (Sort by User, Sort A-Z, and Sort Z-A) customizable via context menus.
- **Workflow Build Fixes**: Resolved matrix value evaluation error in GitHub Actions release workflow.

### v0.2.2
* **Author**: Anthony Mendez
- **Google C++ Style Guide Alignment**: Refactored the entire codebase (classes, headers, pointers, namespaces, and variables) to comply with the Google C++ Style Guide.
- **Improved About Dialog**: Embedded a dynamic, scrollable version log inside the Tools menu's About dialog.
- **Author Identity Alignment**: Synchronized commits and contributors to use full contributor names.

### v0.2.1
* **Author**: Anthony Mendez
- **Automated Linux Scripting**: Introduced `scripts/build-linux.sh` for easy package management and local user installs.
- **NTFS Compilation Fix**: Handled GNU BFD linker crashes on NTFS mounted partitions by automatically fallback linking with `ld.lld`.
- **Tools Menu Integration**: Appended a shortcut action in the OBS Studio main Tools dropdown triggering an interactive "About" QMessageBox.
- **Metadata Centralization**: Restructured project metadata and version properties into `buildspec.json`, generating `version.h` headers dynamically via CMake.

### v0.2.0
* **Contributors**: Anthony Mendez, TheThirdRail, John Titor, DigitOtter, Marcelo dos Santos Mafra
- **Cross-Platform Compilation Support**: Standardized build targets for macOS and Linux compilation using `libobs` and `obs-frontend-api` dependencies.
- **macOS Universal Binaries**: Enabled multi-architecture compilation (`x86_64;arm64`) and Qt 6 framework linking on macOS.
- **GitHub Actions Workflows**: Added automated CI pipelines using OBS plugin-template to package builds for Windows, macOS, and Linux.

### v0.1.9 / v0.1.12
* **Contributor**: DigitOtter
- **macOS Qt 6.8 compatibility**: Resolved issues with `install-qt-action` architectures and compilation dependencies for recent Qt versions.

### v0.1.8
* **Contributor**: DigitOtter
- **Button-driven Reordering**: Wired new Move Up and Move Down toolbar buttons to allow direct manual sorting in the scene tree dock.
- **Theme-aware Toolbar Icons**: Added class mapping to dynamically adjust custom tool icons (plus, trash, up, down) on theme changes.
- **Folder Creation Heuristics**: Added naming validation to generate unique folder names (e.g. `Folder 1`, `Folder 2`) preventing duplicate folder conflicts.

### v0.1.7
* **Contributors**: DigitOtter, Marcelo dos Santos Mafra, Borlader
- **OBS Studio 32+ Support**: Updated scene renaming signals and pointer references to integrate with OBS 28/29 APIs.
- **Russian Localization**: Added Russian translation support (`ru-RU.ini`).
- **Global Duplicate Folder Check**: Enhanced uniqueness validation to scan the entire tree rather than just the active directory branch.
- **Initial Dock Behavior**: Configured the scene tree dock to launch hidden on its first startup.

### v0.1.6
* **Contributor**: DigitOtter
- **Content Sizing & Resizing**: Resolved dock layout issues and fixed list container display constraints.
- **Checksum Utility**: Added checksum generator scripts (`scripts/checksum.bat`) to verify zip packages during Windows releases.

### v0.1.5
* **Contributor**: DigitOtter
- **Windows Compilation Guide**: Added comprehensive development documentation for building on Windows with CMake.
- **Button Class Synchronization**: Synchronized dynamic properties and layout definitions between Qt forms and standard OBS actions.

### v0.1.0 (Initial Release)
* **Contributors**: DigitOtter, Marcelo dos Santos Mafra
- **Hierarchical Dock Interface**: Designed the custom tree structure layout in OBS Studio to replace the linear scene list.
- **Nesting & Drag-and-Drop**: Implemented folder nodes, child scene grouping, and manual drag-and-drop index sorting.
- **Locale Framework**: Setup the initial Brazilian Portuguese (`pt-BR.ini`) and English translation profiles.
