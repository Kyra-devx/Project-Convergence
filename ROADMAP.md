# Roadmap — Project Convergence

Status describes repository evidence. A checked item means the corresponding code or artifact exists; it does not imply device-level, release, or performance validation unless stated.

## Phase A — Game Design Document

- [x] Executive summary and design pillars
- [x] Redstone goals and determinism requirements
- [x] Hybrid combat direction
- [x] Hardcore mode goals
- [x] Hybrid biome and world-generation direction
- [x] Initial GDD v0.1
- [ ] Community review and measurable specification

## Phase B — Android and Voxel Engine Foundation

### B.1 — Foundation

- [x] Android application scaffold (Min SDK 24)
- [x] JNI lifecycle bridge
- [x] CMake and native C++17 build configuration
- [x] Initial standalone voxel data model

### B.2 — Face Culling and Basic Lighting

- [x] Chunk data model sized 16 × 16 × 16
- [x] Skip air blocks during mesh generation
- [x] Emit faces only when the in-chunk neighbor is air
- [x] Keep boundary faces visible when neighbor chunk data is unavailable
- [x] Vertex layout with position, normal, and RGB color
- [x] Grass, dirt, and stone face colors
- [x] GLSL ES 3.0 vertex and fragment shaders
- [x] Directional diffuse lighting with ambient floor
- [x] VAO/VBO upload with GL_STATIC_DRAW and one draw call for the demo chunk
- [x] Static demo camera and MVP matrix to frame the block stack
- [x] Native tests for empty, adjacent, single, and fully solid chunks
- [x] Winding/normal consistency test for all six face directions
- [ ] Verify Android Gradle/NDK build in CI
- [ ] Run and visually inspect on physical Android devices

**B.2 boundary note:** faces on chunk edges are retained because cross-chunk neighbor data is not yet implemented. This avoids holes at exposed edges in a single-chunk scene, but neighboring solid chunks may temporarily produce duplicate internal boundary faces.

### B.3 — Camera and Input

- [ ] First-person camera
- [ ] Touch look and movement controls
- [ ] Camera collision and sensitivity settings

### B.4 — World Generation

- [ ] Deterministic terrain noise
- [ ] Biome selection and terrain shaping
- [ ] Seed/version persistence

### B.5 — Chunk Streaming

- [ ] Neighbor-aware face culling across chunk boundaries
- [ ] Chunk loading and unloading
- [ ] Chunk revision tracking and mesh invalidation
- [ ] Async mesh generation with safe GPU upload scheduling

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
