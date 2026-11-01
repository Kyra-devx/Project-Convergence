# Roadmap — Project Convergence

Status describes repository evidence. A checked item means a scaffold or artifact exists; it does not imply device-level or performance validation unless stated.

## Phase A — Game Design Document

- [x] Executive summary and design pillars
- [x] Redstone goals and determinism requirements
- [x] Hybrid combat direction
- [x] Hardcore mode goals
- [x] Hybrid biome and world-generation direction
- [x] Initial GDD v0.1
- [ ] Community review and measurable specification

## Phase B — Android and Voxel Engine Foundation

- [x] Android application scaffold (Min SDK 24)
- [x] OpenGL ES 3.0 surface and native frame-clear entrypoint
- [x] JNI lifecycle bridge
- [x] Initial standalone voxel data model
- [ ] Verify Gradle, NDK, and CMake build in CI
- [ ] Run on physical devices and record GPU compatibility
- [ ] Face-culling chunk meshing
- [ ] First-person camera and touch controls
- [ ] Deterministic noise-based terrain generation
- [ ] Chunk streaming, loading, and unloading
- [ ] World persistence and save-file versioning

## Phase C — Redstone Simulation

- [ ] Versioned behavioral reference specification
- [ ] Block update queue and deterministic ordering
- [ ] Tick scheduler targeting 20 ticks per second
- [ ] Power propagation and component behavior
- [ ] Quasi-connectivity where applicable to the chosen reference
- [ ] Data-driven test format and automated runner
- [ ] Expand parity suite toward 700+ cases
- [ ] Benchmark simulation and worst-case contraptions

## Phase D — Combat System

- [ ] Attack timing and damage formulas
- [ ] Shield and blocking behavior
- [ ] Sweeping attack behavior
- [ ] Mobile touch layout and accessibility
- [ ] Network latency and authoritative hit resolution
- [ ] Combat regression tests

## Phase E — Compatibility Layer

- [ ] Versioned content schema
- [ ] Datapack-style parser
- [ ] Add-on/content pack import path
- [ ] Compatibility matrix and unsupported-feature reporting
- [ ] Sandboxed scripts and resource limits
- [ ] Adapters for selected APIs
- [ ] Security review before executing third-party content

> Compatibility is staged. Arbitrary Fabric mods or Java bytecode are not promised to run directly inside the native C++ runtime.

## Phase F — Polish and Community

- [ ] Complete developer documentation
- [ ] CI build and test gates
- [ ] Performance budgets for representative Android devices
- [ ] Internal alpha
- [ ] Public beta
- [ ] Community feedback and release process

## Release gates

A phase is not complete solely because source files exist. Completion requires reproducible build instructions, automated checks where practical, documented test results, and review of known limitations.
