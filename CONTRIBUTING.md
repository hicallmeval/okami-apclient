# Contributing to Okami APClient

Thanks for your interest in contributing! There are many ways to help improve this project, whether you write code or not.

## Ways to Contribute

### **Found a Bug?**

Use our [Bug Report template](https://github.com/Axertin/okami-apclient/issues/new?template=bug_report.md) to let us know what's wrong.

### **Discovered Memory Mappings?**

Help us understand Okami's save data! Use the [Memory ID Mapping template](https://github.com/Axertin/okami-apclient/issues/new?template=memory_id_mapping.md) to report what game flags mean.

### **Want a New Feature?**

Use our [Feature Request template](https://github.com/Axertin/okami-apclient/issues/new?template=feature_request.md) to suggest improvements to the client.

### **Discussion & Help**

Join us in the [Archipelago Discord](https://discord.com/channels/731205301247803413/1196620860405067848) for general discussion and questions.

### **Want to Code?**

Keep reading! We'd love your help.

## Getting Started with Development

### Prerequisites

- Visual Studio 2019/2022 (or Windows SDK + Clang/MSVC)
- CMake 3.21+
- Ninja
- Git with submodules support

### Quick Setup

```bash
git clone --recursive https://github.com/Axertin/okami-apclient.git
cd okami-apclient
cmake --preset x64-clang-debug
cmake --build --preset x64-clang-debug
```

See the [Development Guide](docs/development.md) for detailed build instructions.

## Code Contributions

### Before You Start

1. **Check existing issues** - Someone might already be working on it
2. **Open an issue first** for larger changes to discuss the approach
3. **Fork the repository** or create a feature branch

### Development Workflow

1. **Create a branch** - `git checkout -b feature/your-feature-name`
2. **Make your changes** - Keep commits focused and atomic
3. **Format your code** - Run `./format.sh` (use Git Bash on Windows)
4. **Test your changes** - Make sure the mod loads and works as expected
5. **Submit a pull request** - Use the PR template when available

### Code Guidelines

- **Run `./format.sh` before committing.** CI fails on unformatted code.
- **Ensure your changes don't crash the game.** Unit tests welcome where they make sense; see [docs/development.md](docs/development.md) for the writing-tests guide.
- **Respect the architectural boundary.** Game-side definitions (enums, struct layouts, MSD/save formats) live in `include/okami/`; Archipelago-side code (sockets, managers, UI) lives in `src/okami-apclient/`.
- **Update the relevant doc if you're changing observable behaviour.**

Full toolchain, formatting, and code-style details are in [docs/development.md](docs/development.md).

## Detailed Documentation

For more in-depth information, check the `docs/` folder:

- **[Development Guide](docs/development.md)** - Toolchain, build presets, code style, writing tests, debugging.
- **[Architecture](docs/architecture.md)** - Source tree, managers, and general structure.
- **[Mod Flow](docs/mod-flow.md)** - Runtime trace from launch to first check.
- **[Server Protocol](docs/server-protocol.md)** - Socket and APWorld API reference (location/item ID schemes, slot_data, version compatibility).
- **[Save System](docs/save-system.md)** - SaveMan deep-dive.
- **[Memory Mapping Guide](https://github.com/Axertin/wolf/blob/master/docs/memory-mapping.md)** - How to contribute reverse engineering discoveries (in WOLF repo).

## Questions?

- **General discussion**: [Archipelago Discord Thread](https://discord.com/channels/731205301247803413/1196620860405067848)
- **Bug reports**: Use the issue templates
- **Development questions**: Open a discussion / issue or ask in Discord

Thanks for contributing!
