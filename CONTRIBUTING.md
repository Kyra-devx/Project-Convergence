# Contributing to Project Convergence

Project Convergence is an experimental voxel-engine and game-design effort. Correctness, reproducibility, and clear scope are more valuable than large unreviewable changes.

## Before starting

1. Read README.md, docs/GDD.md, and docs/ARCHITECTURE.md.
2. Check ROADMAP.md and open issues to avoid duplicate work.
3. Discuss major mechanics or architecture changes before implementation.
4. Do not include proprietary game assets, extracted game data, private keys, personal data, or unlicensed third-party content.

## Development setup

- JDK 17
- Android SDK Platform 35
- Android NDK 27.0.12077973 (or compatible NDK r25+)
- CMake 3.22.1
- Android Studio with Android Gradle Plugin 8.7.x support
- Device or emulator with OpenGL ES 3.0

The initial scaffold does not include the Gradle Wrapper. Record exact local tool versions when reporting a build result.

## Branch and commit workflow

- Create focused branches from the current default branch.
- Use descriptive branch names such as feature/chunk-meshing or fix/jni-lifecycle.
- Keep commits small and explain the reason for each change.
- The repository audit policy requires signed commits. Configure a signing key in Git and verify that the commit is marked verified where supported.
- Never commit generated build output, signing credentials, local SDK paths, or machine-specific configuration.

## Code conventions

### C++

- Use C++17 for the current native scaffold.
- Prefer explicit ownership and RAII.
- Keep platform-specific code behind narrow interfaces.
- Avoid hidden global mutable state in simulation code.
- Document coordinate systems, thread ownership, and deterministic ordering.

### Java

- Use Java 17 compatibility.
- Keep Android lifecycle/UI separate from simulation rules.
- Do not perform long-running world or meshing work on the UI thread.
- Check JNI boundaries for nullability, ownership, and thread affinity.

### Documentation

- Label implementation status honestly: planned, scaffolded, implemented, or verified.
- Record reference game/version semantics when discussing compatibility.
- Include test evidence and known limitations in pull requests.

## Tests and pull requests

A pull request should include the problem, proposed solution, issue links where relevant, build/test commands and results, screenshots for visible Android changes, performance measurements for hot loops, and compatibility notes for save formats or simulation behavior.

Do not mark an unexecuted test as passing. Code review is not a substitute for device or performance validation.

## Security and third-party content

Treat third-party content as untrusted. Importers and scripting systems must validate input sizes, paths, resource usage, and version compatibility. Never execute imported scripts with unrestricted host access.

By contributing, you confirm that your contribution can be distributed under the repository's applicable license and that you have the right to submit it.
