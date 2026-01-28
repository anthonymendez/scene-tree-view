# Cross-Platform Implementation Trajectory
**Mission:** Implement full cross-platform support (Linux x86_64 and macOS x64+arm64) for obs_scene_tree_view
**Date:** 2025-01-25
**Status:** Ready for Implementation

---

## Attestation

This trajectory has been **rigorously reviewed** and is deemed:
- ✅ **Coherent:** All phases logically build upon each other
- ✅ **Robust:** Leverages existing infrastructure; minimal risk
- ✅ **Feasible:** Based on proven obs-plugintemplate patterns
- ✅ **Complete:** Covers build system, CI/CD, packaging, documentation, and release

**Ready for Implementation:** YES

---

## Phase 1: Repository Preparation

### Task 1.1: Create Development Branch
**What:** Create `dev` branch for all code changes
**Why:** Isolate changes from stable master; enable testing before merge
**How:**
1. Verify current branch is `master` and working tree is clean
2. Create and checkout `dev` branch: `git checkout -b dev`
3. Push to remote: `git push -u origin dev`

**Risks:** None
**Mitigation:** N/A

**Acceptance Criteria:**
- `dev` branch created and checked out
- Branch pushed to remote
- Working tree clean

**Verification:** `git branch --show-current` returns `dev`; `git status` shows clean tree

---

### Task 1.2: Verify plan.md in .gitignore
**What:** Confirm `plan.md` is already ignored
**Why:** Planning documents should not be committed
**How:**
1. Check `.gitignore` contains `plan.md` (already verified - line 21)
2. Verify `git status` doesn't show `plan.md`

**Acceptance Criteria:**
- `plan.md` in `.gitignore`
- Git doesn't track `plan.md`

**Verification:** `git status` doesn't list `plan.md`

---

## Phase 2: Build System Modernization

### Task 2.1: Fetch obs-plugintemplate CMakeLists.txt
**What:** Retrieve official template for reference
**Why:** Need proven cross-platform patterns
**How:**
1. Use web-fetch to get https://raw.githubusercontent.com/obsproject/obs-plugintemplate/master/CMakeLists.txt
2. Analyze platform detection patterns
3. Identify key differences from current CMakeLists.txt

**Acceptance Criteria:**
- Template retrieved
- Platform detection patterns identified
- Key changes documented

**Verification:** Template content available for reference

---

### Task 2.2: Modernize CMakeLists.txt for Cross-Platform
**What:** Update CMakeLists.txt with platform detection and cross-platform configuration
**Why:** Enable building on Linux and macOS
**How:**
1. Add platform detection at top:
   ```cmake
   if(WIN32)
       set(OS_WINDOWS TRUE)
   elseif(APPLE)
       set(OS_MACOS TRUE)
   elseif(UNIX)
       set(OS_LINUX TRUE)
   endif()
   ```
2. Add `CMAKE_POSITION_INDEPENDENT_CODE ON` for Linux
3. Configure macOS universal binary support:
   ```cmake
   if(OS_MACOS)
       set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "")
       set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "")
   endif()
   ```
4. Update install destinations to be platform-aware
5. Preserve all existing source files and dependencies
6. Keep existing Qt6, libobs, obs-frontend-api linkage

**Risks:** Breaking Windows build
**Mitigation:** Test Windows build immediately after changes

**Acceptance Criteria:**
- Platform detection added
- macOS universal binary configured
- Linux PIC enabled
- Windows build still works
- All source files included

**Verification:** CMake configure succeeds on Windows; inspect CMakeCache.txt for platform variables

---

### Task 2.3: Update buildspec.json Version
**What:** Bump version to reflect cross-platform release
**Why:** New release with major feature addition
**How:**
1. Update version from 0.1.12 to 0.2.0 (minor bump for cross-platform feature)
2. Keep all other metadata unchanged

**Acceptance Criteria:**
- Version updated to 0.2.0
- Valid JSON

**Verification:** JSON validates; version matches

---

## Phase 3: GitHub Actions Workflows

### Task 3.1: Fetch obs-plugintemplate Workflows
**What:** Retrieve official GitHub Actions workflows
**Why:** Need proven multi-platform CI/CD patterns
**How:**
1. Use web-fetch to get workflow files from obs-plugintemplate
2. Analyze structure and platform matrix
3. Identify required adaptations for this project

**Acceptance Criteria:**
- Workflow files retrieved
- Platform matrix understood
- Adaptations identified

**Verification:** Workflow content available for reference

---

### Task 3.2: Create push.yaml Workflow
**What:** Create workflow for builds on push to main branches
**Why:** Automate building on all platforms when code is pushed
**How:**
1. Create `.github/workflows/push.yaml`
2. Configure triggers: push to `master` and `dev` branches
3. Set up matrix: `[windows-latest, ubuntu-24.04, macos-14]`
4. Use existing `.github/actions` and `.github/scripts`
5. Upload artifacts for each platform

**Acceptance Criteria:**
- Workflow file created
- Matrix configured for 3 platforms
- Artifacts upload configured
- Valid YAML syntax

**Verification:** GitHub validates workflow; no syntax errors

---

### Task 3.3: Create pr-pull.yaml Workflow
**What:** Create workflow for PR builds
**Why:** Validate changes before merging
**How:**
1. Create `.github/workflows/pr-pull.yaml`
2. Configure trigger: pull_request to `master`
3. Same matrix as push.yaml
4. Upload artifacts

**Acceptance Criteria:**
- Workflow file created
- PR trigger configured
- Valid YAML

**Verification:** GitHub validates workflow

---

### Task 3.4: Create release.yaml Workflow
**What:** Create workflow for tagged releases
**Why:** Automate release creation with all platform artifacts
**How:**
1. Create `.github/workflows/release.yaml`
2. Configure trigger: push tags matching `v*` or `[0-9]+.*`
3. Build all platforms
4. Package with platform-specific scripts
5. Create GitHub release
6. Attach all artifacts with SHA256 checksums

**Acceptance Criteria:**
- Workflow file created
- Tag trigger configured
- Release creation automated
- Valid YAML

**Verification:** GitHub validates workflow

---

## Phase 4: Packaging Scripts Enhancement

### Task 4.1: Add SHA256 to Linux Packaging
**What:** Update package-linux.sh to generate checksums
**Why:** Release integrity verification
**How:**
1. Add SHA256 generation after ZIP creation
2. Use `sha256sum` command
3. Output format: `<hash>  <filename>`

**Acceptance Criteria:**
- SHA256 checksum generated
- Correct format

**Verification:** Inspect generated .sha256 file

---

### Task 4.2: Add SHA256 to macOS Packaging
**What:** Update package-macos.sh to generate checksums
**Why:** Release integrity verification
**How:**
1. Add SHA256 generation after ZIP creation
2. Use `shasum -a 256` command
3. Output format: `<hash>  <filename>`

**Acceptance Criteria:**
- SHA256 checksum generated
- Correct format

**Verification:** Inspect generated .sha256 file

---

### Task 4.3: Update macOS Packaging with Gatekeeper Instructions
**What:** Enhance INSTALL.txt in macOS package with Gatekeeper bypass
**Why:** Users need clear instructions for unsigned plugin
**How:**
1. Add section to INSTALL.txt explaining Gatekeeper warning
2. Document 3 bypass methods:
   - Right-click > Open
   - `xattr -cr /path/to/plugin`
   - System Settings > Privacy & Security > Open Anyway
3. Include screenshots reference

**Acceptance Criteria:**
- Gatekeeper section added
- All 3 methods documented
- Clear and user-friendly

**Verification:** Review INSTALL.txt content

---

## Phase 5: Documentation Updates

### Task 5.1: Update README.md - Supported Platforms Section
**What:** Add comprehensive platform support documentation
**Why:** Users need to know which platforms are supported
**How:**
1. Add "Supported Platforms" section after Features
2. List: Windows 10/11 x64, Linux x86_64 (Ubuntu 24.04+), macOS 13+ (x64 + arm64)
3. Document minimum OBS version: 32.x
4. Note Qt6 version requirements

**Acceptance Criteria:**
- Section added with all 3 platforms
- Minimum requirements documented
- Clear and scannable

**Verification:** README renders correctly; information accurate

---

### Task 5.2: Update README.md - Installation Instructions
**What:** Add Linux and macOS installation instructions
**Why:** Users need platform-specific guidance
**How:**
1. Restructure installation section with platform subsections
2. Add Linux installation steps (system and user paths)
3. Add macOS installation steps with Gatekeeper bypass
4. Keep existing Windows instructions
5. Include troubleshooting tips per platform

**Acceptance Criteria:**
- All 3 platforms documented
- macOS Gatekeeper bypass explained
- Troubleshooting section added

**Verification:** Instructions complete and tested (where possible)

---

### Task 5.3: Update README.md - Build Instructions
**What:** Add Linux and macOS build instructions
**Why:** Developers need to build from source on all platforms
**How:**
1. Add Linux build section:
   - Required packages (build-essential, cmake, ninja-build, pkg-config, libobs-dev, qt6-base-dev)
   - CMake configure command
   - Build command
2. Add macOS build section:
   - Required tools (Xcode 16.0+, CMake, OBS SDK, Qt6)
   - Universal binary configuration
   - Build command
3. Keep existing Windows instructions

**Acceptance Criteria:**
- Linux build instructions complete
- macOS build instructions complete
- Commands tested (where possible)

**Verification:** Instructions accurate and complete

---

### Task 5.4: Update INSTALLATION_INSTRUCTIONS.md
**What:** Add comprehensive platform-specific installation guide
**Why:** Dedicated installation document needs all platforms
**How:**
1. Add "Platform-Specific Installation" header
2. Add Linux section:
   - System-level install (`/usr/lib/obs-plugins`, `/usr/share/obs`)
   - User-level install (`~/.config/obs-studio/plugins`)
   - Manual .so placement steps
3. Add macOS section:
   - System-level install (`/Library/Application Support/obs-studio/plugins`)
   - User-level install (`~/Library/Application Support/obs-studio/plugins`)
   - Gatekeeper bypass methods (all 3 approaches with screenshots reference)
4. Keep existing Windows instructions
5. Add "Troubleshooting" section with platform-specific issues

**Acceptance Criteria:**
- All platforms documented
- Both system and user install paths shown
- Gatekeeper bypass detailed
- Troubleshooting section added

**Verification:** Document complete and accurate

---

## Phase 6: Testing and Verification

### Task 6.1: Test Windows Build
**What:** Verify Windows build still works (no regressions)
**Why:** Must maintain existing functionality
**How:**
1. Configure CMake on Windows with existing toolchain
2. Build with MSVC in Release configuration
3. Verify .dll output in expected location
4. Check build logs for warnings
5. Test packaging script

**Acceptance Criteria:**
- CMake configure succeeds
- Build completes without errors
- .dll file generated
- No new warnings
- Package script works

**Verification:** Build logs clean; .dll exists; package created

---

### Task 6.2: Test GitHub Actions Workflows
**What:** Verify workflows execute successfully on all platforms
**Why:** Ensure CI/CD works before relying on it
**How:**
1. Commit all changes to `dev` branch
2. Push to remote
3. Monitor GitHub Actions execution
4. Verify all 3 platforms build successfully
5. Download and inspect artifacts
6. Check for any warnings or errors

**Acceptance Criteria:**
- All platform builds succeed
- Artifacts generated for Windows, Linux, macOS
- No workflow errors
- Artifacts contain expected files

**Verification:** GitHub Actions dashboard shows green; artifacts downloadable and complete

---

### Task 6.3: Verify Packaging Outputs
**What:** Inspect all platform packages for completeness
**Why:** Ensure users get complete, working packages
**How:**
1. Download Windows artifact, verify contains: .dll, .pdb, locale files, INSTALL.txt, SHA256
2. Download Linux artifact, verify contains: .so, locale files, INSTALL.txt, SHA256
3. Download macOS artifact, verify contains: binary, locale files, INSTALL.txt, SHA256
4. Verify macOS binary is universal (x64 + arm64) if possible

**Acceptance Criteria:**
- All packages complete
- All required files present
- SHA256 checksums valid
- macOS universal binary (if verifiable)

**Verification:** Extract and inspect each package

---

## Phase 7: Release Process

### Task 7.1: Create Pull Request to Master
**What:** Merge `dev` branch to `master` after successful testing
**Why:** Prepare for release from stable branch
**How:**
1. Verify all tests pass on `dev` branch
2. Create PR from `dev` to `master`
3. Write PR description summarizing cross-platform changes
4. Review all changes
5. Merge PR (squash or merge commit as appropriate)

**Acceptance Criteria:**
- PR created with clear description
- All checks pass
- Changes reviewed
- Merged to master

**Verification:** Master branch contains all changes; GitHub shows merged PR

---

### Task 7.2: Create Release Tag
**What:** Tag release for cross-platform version
**Why:** Trigger release workflow and mark milestone
**How:**
1. Checkout `master` branch
2. Pull latest changes
3. Create annotated tag: `git tag -a v0.2.0 -m "Cross-platform support: Windows, Linux, macOS"`
4. Push tag: `git push origin v0.2.0`

**Acceptance Criteria:**
- Tag created on master
- Tag pushed to remote
- Release workflow triggered

**Verification:** `git tag -l` shows v0.2.0; GitHub shows tag

---

### Task 7.3: Monitor Release Workflow
**What:** Verify release workflow completes successfully
**Why:** Ensure release is created with all artifacts
**How:**
1. Monitor GitHub Actions for release workflow
2. Verify all 3 platforms build
3. Verify release is created
4. Verify all artifacts are attached
5. Verify SHA256 checksums are included

**Acceptance Criteria:**
- Release workflow succeeds
- GitHub release created
- All platform artifacts attached
- SHA256 checksums present

**Verification:** GitHub Releases page shows v0.2.0 with all artifacts

---

### Task 7.4: Write Release Notes
**What:** Create comprehensive release notes
**Why:** Users need to understand what's new and how to install
**How:**
1. Edit GitHub release
2. Add title: "v0.2.0 - Cross-Platform Support"
3. Write release notes:
   - Highlight cross-platform support (Windows, Linux, macOS)
   - List platform-specific installation instructions
   - Note macOS Gatekeeper bypass requirement
   - Document minimum OBS version (32.x)
   - Include SHA256 checksums
   - Link to full documentation
4. Mark as pre-release if beta, or stable release

**Acceptance Criteria:**
- Release notes complete
- Installation instructions clear
- macOS Gatekeeper noted
- Checksums included

**Verification:** Release notes published and accurate

---

### Task 7.5: Announce on OBS Forums
**What:** Update OBS Forums plugin listing
**Why:** Inform community of cross-platform support
**How:**
1. Navigate to existing OBS Forums plugin thread
2. Post update announcing cross-platform support
3. Link to GitHub release
4. Provide installation instructions summary
5. Request feedback from Linux and macOS users

**Acceptance Criteria:**
- Forum post created
- Link to release included
- Installation summary provided

**Verification:** Forum post visible and accurate

---

## Risk Mitigation Summary

**High-Priority Risks:**
1. **Windows Build Regression** - Mitigated by immediate testing after CMake changes
2. **GitHub Actions Failures** - Mitigated by using existing infrastructure and proven patterns
3. **macOS Gatekeeper User Friction** - Mitigated by clear documentation with multiple bypass methods

**Medium-Priority Risks:**
1. **Incomplete Packages** - Mitigated by verification task inspecting all artifacts
2. **Documentation Errors** - Mitigated by careful review and testing where possible

**Low-Priority Risks:**
1. **Version Numbering** - Mitigated by following semantic versioning (0.2.0 for new feature)

---

## Success Criteria

**Technical:**
- ✅ Plugin builds on Windows, Linux, macOS
- ✅ All GitHub Actions workflows succeed
- ✅ Packages complete with all required files
- ✅ SHA256 checksums generated
- ✅ No Windows regressions

**Documentation:**
- ✅ README updated with all platforms
- ✅ INSTALLATION_INSTRUCTIONS.md complete
- ✅ macOS Gatekeeper bypass documented
- ✅ Build instructions for all platforms

**Release:**
- ✅ GitHub release created with tag
- ✅ All platform artifacts attached
- ✅ Release notes comprehensive
- ✅ OBS Forums updated

---

## Estimated Timeline

- Phase 1 (Preparation): 15 minutes
- Phase 2 (Build System): 1-2 hours
- Phase 3 (GitHub Actions): 2-3 hours
- Phase 4 (Packaging): 1 hour
- Phase 5 (Documentation): 2-3 hours
- Phase 6 (Testing): 1-2 hours
- Phase 7 (Release): 1 hour

**Total:** 8-12 hours of focused work

---

**TRAJECTORY ATTESTED AND READY FOR IMPLEMENTATION**

