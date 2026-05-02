# Development Guide

This guide covers the technical details of developing okami-apclient.

## Prerequisites

### Required Tools

- **Visual Studio 2019/2022** with C++ development workload
  - Or Windows SDK + Clang/MSVC separately
- **CMake 3.21+**
- **Ninja** build system
- **Git** with submodules support

### Recommended: VSCode Setup

For the easiest development experience:

1. **Install VS2022** with C++ development workload (for compiler toolchain)
   - You can also install Ninja, Clang, and the Windows SDK separately, if you prefer.
2. **Install VSCode** or VSCodium, if you prefer
3. **Open folder** in VSCode → repo root directory
   - VSCode will prompt to install recommended extensions - click "Install All"
4. **CMake: Configure** → select preset (`x64-clang-debug`, or your user preset, or similar)
5. **CMake: Build** → select target or "all"

VSCode automatically hides dependency targets and provides a clean interface.

### Alternative: Visual Studio

If you prefer Visual Studio IDE...

When opening the project in Visual Studio, accept the `.vsconfig` prompt to install the Clang toolchain. If it doesn't prompt you, close and reopen the project after the initial load.

For more specific Visual Studio guidance, see [Visual Studio Setup](#visual-studio-setup).

## Building

### Before You Start - Verification

Run these commands to verify your setup:

- `ninja --version` (should show "1.10+" or similar, not "command not found")
- `git --version`
- `cmake --version` (should show "3.21+" or higher)

### Quick Start

```bash
git clone --recursive https://github.com/Axertin/okami-apclient.git
cd okami-apclient
cmake --preset x64-clang-debug
cmake --build --preset x64-clang-debug
```

### Build Presets

- `x64-clang-debug` / `x64-clang-release` - Native Windows build with Clang
- `x64-debug` / `x64-release` - Native Windows build with MSVC
- `llvm-mingw-cross-debug` / `llvm-mingw-cross-release` - Cross-compile the Windows DLL on Linux using llvm-mingw (recommended Linux toolchain)
- `linux-cross-debug` / `linux-cross-release` - Cross-compile via GCC MinGW-w64; requires GCC 13.3+, breaks on stock Ubuntu (kept for niche setups)
- `native-tests-debug` / `native-tests-release` - Linux-native test build; no Windows toolchain needed. Fastest iteration loop for anything testable. The DLL target is excluded; only `apclient-tests` and `apclient-harness-tests` are produced.

### Linux Cross-Compilation

Two toolchain options are available. **llvm-mingw is recommended** — it works on all
distros including Ubuntu, and uses the same Clang compiler used for the Windows builds.

#### Option A: llvm-mingw (Recommended)

[llvm-mingw](https://github.com/mstorsjo/llvm-mingw) is a Clang/LLVM-based MinGW-w64
toolchain that builds cleanly on all Linux distros.

1. Download the latest Linux x86_64 UCRT release from
   [github.com/mstorsjo/llvm-mingw/releases](https://github.com/mstorsjo/llvm-mingw/releases).
   The filename looks like `llvm-mingw-YYYYMMDD-ucrt-ubuntu-20.04-x86_64.tar.xz`.

2. Extract it and set the `LLVM_MINGW_ROOT` environment variable:

   ```bash
   sudo tar -xf llvm-mingw-*-ucrt-ubuntu-20.04-x86_64.tar.xz -C /opt
   sudo mv /opt/llvm-mingw-* /opt/llvm-mingw
   export LLVM_MINGW_ROOT=/opt/llvm-mingw   # add to ~/.bashrc to persist
   ```

3. Build using the `llvm-mingw-cross-*` presets:

   ```bash
   cmake --preset llvm-mingw-cross-debug
   cmake --build --preset llvm-mingw-cross-debug
   ```

#### Option B: GCC MinGW-w64 (GCC 13.3+ required)

```bash
sudo apt install gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 ninja-build
cmake --preset linux-cross-debug
cmake --build --preset linux-cross-debug
```

> **Ubuntu compatibility:** Ubuntu ships GCC 13.2 for `gcc-mingw-w64-x86-64` across all
> releases through 25.10, with no newer version available via apt. GCC 13.2 rejects the
> template-id constructor syntax in `websocketpp` as a hard parse error in C++20 mode
> ([CWG DR 2037][cwgdr2037]) with no pragma-level workaround. The CMake configure step
> will fail with an actionable error message on unsupported compilers.
> **Fedora** (and other distros that ship GCC 13.3+) cross-compile without issue.

[cwgdr2037]: https://cplusplus.github.io/CWG/issues/2037.html

### Custom Ninja Path

If Ninja isn't in your PATH, create `CMakeUserPresets.json`:

```json
{
  "version": 3,
  "configurePresets": [
    {
      "name": "my-local-debug",
      "inherits": "x64-clang-debug",
      "cacheVariables": {
        "CMAKE_MAKE_PROGRAM": "D:/path/to/ninja/ninja.exe"
      }
    }
  ]
}
```

### Visual Studio Setup (Detailed)

Visual Studio's CMake integration can be tricky with dependency-heavy projects. Follow these steps for the best experience:

#### Initial Setup

1. **Open Visual Studio** (if opening from start menu, select "Continue without code")
2. **Open the project**: File → Open → CMake... → select the repository's root `CMakeLists.txt`
   - **Do NOT** open any `.sln` files in the `external/` folder
3. **Accept the `.vsconfig` prompt** to install the Clang toolchain
   - If no prompt appears, close and reopen the project after initial configuration

#### Configuration and Building

1. **Wait for initial configure** to complete (watch the Output window)
2. **Select build preset** in the toolbar: choose `x64-clang-debug` or `x64-clang-release`
3. **Select build target** in the dropdown next to the build button:
   - **Main targets**: `okami-apclient`, `apclient-tests`, `apclient-harness-tests`
   - **Ignore**: All the dependency targets (imgui examples, vcpkg packages, etc.)
   - **Tip**: The target dropdown will be very long due to dependencies - scroll to find your targets

#### Target Selection Tips

- **Default startup**: The first target alphabetically will be selected initially
- **Main targets to use**:
  - `okami-apclient` - The main mod DLL
  - `apclient-tests` - Unit test suite
  - `apclient-harness-tests` - End-to-end fixture suite
- **Avoid**: Any targets with names like `example_*`, `*_test`, or vcpkg package names

#### Building

- **Single target**: Select target → press F7 or click Build
- **All targets**: Use the command line instead: `cmake --build --preset x64-clang-debug`
- **Clean build**: Build → Clean All, then rebuild

#### Troubleshooting

- **"Target not found" errors**: Make sure you selected the root `CMakeLists.txt`, not a subdirectory
- **Clang not working**: Ensure the Clang toolchain is installed via the VS Installer
- **Too many targets**: This is normal - dependencies expose many targets that can't be hidden in VS
- **Git submodules missing**: Run `git submodule update --init --recursive`
- **vcpkg errors**: Delete `out/` folder and reconfigure

## Code Style & Formatting

### Automatic Formatting

We use clang-format for consistent code style. **Always run before committing:**

```bash
./format.sh
```

Use Git Bash on Windows if needed.

### Code Standards

- **C++23** standard.
- **Descriptive names** — prefer descriptive names over abbreviations.
- **Type safety** — prefer `MemoryAccessor<T>` over raw pointer arithmetic for game memory.
- **Cross-platform** — avoid Windows-API calls Proton doesn't implement. The `llvm-mingw-cross-debug` preset will catch the link errors.

## Project Structure

### Key Directories

- `src/okami-apclient/` - Main mod DLL code
- `include/okami/` - Game-specific data structures (enums, bitfields, game state definitions)
- `external/` - Dependency Git submodules
- `cmake/` - Build system utilities
- `tests/` - Unit tests

### Target Architecture

- **okami-apclient.dll** - Main mod injected into the game (shared library)
- **apclient-tests** - Unit test runner (Catch2)
- **apclient-harness-tests** - End-to-end fixture runner (Catch2)

## Dependencies

Dependencies are managed through vcpkg and git submodules:

- **vcpkg packages** - Things published on vcpkg (nlohmann-json, asio, openssl, minhook, etc.)
- **Git submodules** - Everything else (apclientpp, wswrap, websocketpp, imgui)

Key dependencies from the project:

- [apclientpp](https://github.com/black-sliver/apclientpp) - Archipelago client library
- [imgui](https://github.com/ocornut/imgui) - Immediate mode GUI

All dependencies are automatically handled by the build system.

## Memory Management Guidelines

### Game Memory Access

- WOLF provides `MemoryAccessor<T>` for typed memory access - refer to WOLF documentation for details
- **Never assume memory is valid** - always check bounds
- Game state structures are defined in `include/okami/gamestate/` headers

## Testing

### Running tests

Two test executables are built by every preset that includes tests:

- **`apclient-tests`** — unit suite (Catch2). One `test_*.cpp` per component at the top of `tests/`.
- **`apclient-harness-tests`** — end-to-end fixtures. Lives in `tests/harness/`.

Linux iteration loop (no Windows toolchain needed):

```bash
cmake --preset native-tests-debug
cmake --build build/native-tests-debug
./build/native-tests-debug/tests/apclient-tests             # all tests
./build/native-tests-debug/tests/apclient-tests "[regression]"   # filter by Catch2 tag
./build/native-tests-debug/tests/apclient-tests "test name"      # filter by name substring
```

Windows native: `cmake --build --preset x64-clang-debug && ./build/x64-clang-debug/apclient-tests.exe` (and `apclient-harness-tests.exe`).

### Writing tests

Tests use Catch2 plus the in-tree mock framework at `tests/mocks/`. The mock provides:

- `wolf::mock::mockMemory` — a fake byte array that stands in for `main.dll`'s address space.
- `wolf::mock::reserveMemory(size)` — grows the array to fit your test data.
- A hook registry — `wolf::hookFunction` records the hook function pointer; `wolf::mock::triggerHook<Fn>(offset, args...)` invokes it directly so your test exercises the hook body without a real game.
- Stubs for `wolf::giveItem`, the lifecycle callbacks (`onGameTick`, `onPlayStart`, etc.), `wolf::createBitfieldMonitor`, and the logging helpers.

The pattern for tests that need realistic game memory: call `wolf::mock::reserveMemory(largeOffset + sizeof(T))`, write through `apgame::*` accessors, and assert on the values via the same accessors. `test_saveman.cpp` and `test_brush_regression.cpp` are good references.

### Manual testing

Before submitting a PR with non-trivial changes:

1. Code builds without errors on `native-tests-debug` and at least one of `llvm-mingw-cross-debug` / `x64-clang-debug`.
2. `./format.sh` is clean (CI will fail otherwise).
3. The mod loads in-game without crashing.
4. The feature works as intended.

For network-related changes, also test against a real Archipelago server with a connected slot. Watch the in-game console for errors. Debug logs are written to `logs/` with timestamps.

## Common Development Tasks

### Adding New Game State Definitions

1. See WOLF for details on how to do this

### Adding New Archipelago Checks or Rewards

1. Define the check/reward data in the appropriate location system
2. Add handler logic in the reward or check manager
3. Add unit tests in `tests/` to verify behavior

## Debugging

### TDR: Why Debugger Attach Crashes the Game

The crash is caused by **Windows TDR (Timeout Detection and Recovery)**:

1. Debugger attach (`DebugActiveProcess`) suspends all threads
2. The render thread stops mid-frame, stalling the GPU command queue
3. After ~2 seconds (default `TdrDelay`), Windows resets the GPU
4. The D3D11 device becomes invalid (`DXGI_ERROR_DEVICE_REMOVED`)
5. The game has no device-lost recovery — next D3D11 call crashes the process

### TDR Workaround

Run as admin, then **reboot**:

```bat
:: Increase TDR timeout to 120 seconds (TDR still works for real hangs)
reg add "HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers" /v TdrDelay /t REG_DWORD /d 120 /f

:: OR fully disable TDR (dev machines only)
reg add "HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers" /v TdrLevel /t REG_DWORD /d 0 /f
```

To restore defaults:

```bat
reg add "HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers" /v TdrDelay /t REG_DWORD /d 2 /f
reg add "HKLM\SYSTEM\CurrentControlSet\Control\GraphicsDrivers" /v TdrLevel /t REG_DWORD /d 3 /f
```

### Local Debugging (Windows)

Prerequisites:
- Install the [CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb)
  VSCode extension (listed in `.vscode/extensions.json`)
- Apply the TDR workaround above

Workflow:

1. Build and install: run the **CMake: install** task (or use the launch config that does it
   automatically)
2. Launch the game via WOLF
3. In VSCode, select **"Attach to Okami (LLDB)"** and press F5
4. Pick the game process from the list
5. Set breakpoints in `okami-apclient` source files

The PDB is in the build directory and its path is embedded in the DLL, so LLDB finds symbols
automatically.

### Remote Debugging

For debugging the game remotely:

Install LLDB (included in the
[LLVM installer](https://github.com/llvm/llvm-project/releases), check the "LLDB" component).
Then start the debug server:

```bat
lldb-server gdbserver --attach <GAME_PID> 0.0.0.0:12345
```

Where `<GAME_PID>` is the PID of the game process.

select **"Remote Attach to Okami"** in the VSCode Run
panel and press F5. Enter the Windows machine's IP and port when prompted.

This config uses the active CMake preset's build output for symbols (`target.exec-search-paths`), so make sure that
the same binary you've just built remotely is deployed to the machine running the game.

## CI/CD Integration

### GitHub Actions

- Builds run on Windows with multiple configurations
- Tests are automatically executed
- Artifacts are generated for releases
- **Ensure formatting** - CI will fail if code isn't formatted properly

## Getting Help

- **Build issues** - Check vcpkg and CMake output
- **Runtime crashes** - Check log files and see [Debugging](#debugging)
- **Community discussion** - [Archipelago Discord](https://discord.com/channels/731205301247803413/1196620860405067848)
