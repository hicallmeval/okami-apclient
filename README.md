# Okami-apclient

A mod for the Steam release of Ōkami HD that lets the game participate in an [Archipelago](https://archipelago.gg) multiworld randomizer. Pair it with the [Okami APWorld](https://github.com/Ragmoa/Archipelago/) on the server side; both are required.

This mod is loaded by the [WOLF](https://github.com/Axertin/wolf) framework, which handles DLL injection and exposes the game's memory, hooks, and UI surface.

## Status

**v0.x.x pre-release.** Expect breaking changes between versions; not recommended for live syncs yet. **v1.0.0** will be the first stable, sync-ready release.

What works right now:

- Connecting to an Archipelago server, picking a slot, sending and receiving items.
- Container randomization, brush randomization, shop randomization.
- AP-isolated saves (`.OKAMI` files; vanilla saves are untouched).

For a full picture of what the mod actually does at runtime, see [docs/mod-flow.md](docs/mod-flow.md).

## Installing

1. Download the latest release from [the releases page](https://github.com/Axertin/okami-apclient/releases)
2. Unzip the release into the mods/ directory, located in the game's installation directory
3. The file structure should look like this:

   ```
   Okami/
   ├── mods/
   │   └── apclient/
   │       ├── game-data/
   │       ├── okami-apclient.dll
   │       └── cacert.pem
   ├── wolf-loader.exe
   ├── dinput8.dll
   ├── okami.exe (and the rest of the vanilla files)
   ```

4. Launch WOLF (reference [WOLF's quickstart](https://github.com/Axertin/wolf/blob/master/README.md) for exact instructions by platform)

Keybindings, console commands, save management, and troubleshooting are covered in [docs/usage.md](docs/usage.md).

## Building From Source

For development setup and detailed build instructions, see [docs/development.md](docs/development.md).

### Quick Start

```bash
git clone --recursive https://github.com/Axertin/okami-apclient.git
cd okami-apclient
cmake --preset x64-clang-debug
cmake --build --preset x64-clang-debug
```

### Prerequisites

- **Visual Studio 2019/2022** or Windows SDK + Clang/MSVC
- **CMake 3.21+**, **Ninja**, **Git** with submodules support

Dependencies are automatically handled through vcpkg and git submodules.

For first-time contributors, detailed setup instructions, and troubleshooting, see the [development guide](docs/development.md).

## Contributing

Contributions are welcome! Please:

1. Read [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/development.md](docs/development.md)
2. Fork the repository
3. Create a feature branch from `master`
4. Test your changes and run `format.sh` (use Git Bash on Windows)
5. Submit a pull request

## Project Structure

- `src/okami-apclient/` - Main mod DLL (managers, sockets, UI, AP-side code)
- `include/okami/` - Game-side data: enums, bitfields, struct layouts, save and MSD formats
- `tests/` - Unit tests (`apclient-tests`) and end-to-end fixtures (`apclient-harness-tests`)
- `docs/` - Architecture, runtime trace, server protocol, save system, dev guide, usage
- `scripts/` - Helper scripts for build-time code generation
- `external/` - Dependency Git submodules (apclientpp, wswrap, websocketpp, imgui, vcpkg)
- `cmake/` - Build system utilities

## Acknowledgements

Projects and people who aren't listed as contributors but have been invaluable as references or assistance

- **Shintensu**'s [OriginEdit](https://github.com/Shintensu/OriginEdit)
- **whataboutclyde**'s [okami-utils](https://github.com/whataboutclyde/okami-utils)
- All of the wonderful contributors to the [Okami Reverse Engineering Wiki](https://okami.speedruns.wiki/Reverse_Engineering)
- Loader and APClient icon: **@sidorak26** on the Archipelago Discord

## License

This project is under the MIT License - see the [LICENSE](LICENSE) file for details.
