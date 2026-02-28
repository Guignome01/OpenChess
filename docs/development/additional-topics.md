# Additional Topics

## AI-Assisted Development

This project provides context files for AI coding assistants:

| File | Purpose |
|------|---------|
| `.github/copilot-instructions.md` | GitHub Copilot instructions |
| `.claude/CLAUDE.md` | Claude Code instructions |

Both files are identical (sync directive at the top of each). They contain the project architecture summary, conventions, patterns, and engineering principles.

### Workflow: Consult Documentation First

Before analyzing or modifying any component, read the relevant `docs/` file for the area you're about to touch. This ensures you understand existing design decisions, naming conventions, and constraints before making changes.

| Task area | Read first |
|-----------|------------|
| Class hierarchy, data flows, concurrency | `docs/development/architecture.md` |
| Coding conventions, naming, frontend patterns | `docs/development/project-standards.md` |
| API endpoints | `docs/development/api.md` |
| File layout | `docs/development/project-structure.md` |
| Build setup, dependencies | `docs/development/installation.md` |
| Board interactions, menus, calibration | `docs/guides/` |
| Hardware, wiring, components | `docs/hardware/` |

### Documentation Sync Rule

When a code change affects architecture, public APIs, endpoints, configuration, build pipeline, file structure, or engineering conventions, update the relevant documentation in the same change — never defer to a follow-up:

- New or changed API endpoints → update [api.md](api.md)
- New or removed source files → update [project-structure.md](project-structure.md)
- Build or dependency changes → update [installation.md](installation.md)
- Architecture or internal design changes → update [architecture.md](architecture.md)
- New LED behaviors, menu changes, or physical interaction changes → update the relevant file in `docs/guides/`
- Chess logic changes in `lib/core/` → update or add unit tests in `test/`

## CLI Quick Reference

PlatformIO CLI is not on `PATH` by default. Use the full path:

| Platform | Path |
|----------|------|
| Windows | `%USERPROFILE%\.platformio\penv\Scripts\pio.exe` |
| Linux | `~/.platformio/penv/bin/pio` |

Common commands:

| Action | Command |
|--------|---------|
| Build | `pio run` |
| Upload firmware | `pio run -t upload` |
| Serial monitor | `pio device monitor` |
| Clean build | `pio run -t clean` |
| Run all tests | `pio test -e native` |
| Run one test suite | `pio test -e native -f test_core` |

Serial monitor runs at **115200 baud** (configured in `platformio.ini`).
