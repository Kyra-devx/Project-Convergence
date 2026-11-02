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

- [x] Initial chunk data model sized 16 × 16 × 16 (expanded to 16 × 128 × 16 in B.6)
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

### B.3 — First-Person Camera and Touch Controls

- [x] Native Camera class with position, yaw, pitch, and front vector
- [x] GLM view and perspective projection matrices
- [x] Look sensitivity and pitch clamp
- [x] Frame-independent movement using delta time
- [x] Delta-time clamp and frame-clock reset on lifecycle pause/resume
- [x] Horizontal joystick movement with diagonal speed normalization
- [x] JNI look and joystick input callbacks
- [x] Mutex-protected input state between UI and GL threads
- [x] Multi-touch pointer tracking (left joystick + right look simultaneously)
- [x] Floating virtual joystick overlay
- [x] Demo camera starts facing the B.2 block stack
- [ ] Verify Android Gradle/NDK build in CI
- [ ] Verify multi-touch behavior and camera feel on physical devices

**B.3 limitations at its original phase:** movement initially lacked collision, gravity, jumping, sprinting, and camera sensitivity settings. B.4 later added basic gravity and heightmap floor collision; jumping and sprinting remain future work.

### B.4 — World Generation

- [x] Deterministic heightmap noise with FastNoiseLite
- [x] Secondary OpenSimplex2 detail noise
- [x] Grass, dirt, and stone surface layering
- [x] Vertical chunk sections for terrain heights above Y=16
- [x] Runtime seed parameter (prototype default: 12345)
- [x] Gravity and heightmap-based floor collision
- [ ] Apply biome selection to surface materials and terrain shaping
- [ ] Persist world seed and generator version

**B.4 limitations at its original phase:** generation initially rendered one X/Z column. B.5 added horizontal streaming, and B.6 consolidated the five vertical sections into a single 16 × 128 × 16 chunk. Biome noise is configured but not yet applied. The seed remains a native constant, not a UI setting or saved-world property. Android build and device verification remain pending.

### B.5 — Chunk Streaming

- [x] ChunkManager keyed by signed X/Z chunk coordinates
- [x] Load chunks around the player and unload distant chunks with hysteresis
- [x] Incremental nearest-first loading budget per frame
- [x] Neighbor-aware face culling across X/Z boundaries
- [x] Neighbor-aware culling between the five vertical terrain sections (superseded by the single 128-block-high column in B.6)
- [x] Rebuild adjacent meshes when a neighbor loads or unloads
- [x] CPU-only chunk ownership with RAII and renderer-owned GPU resources (B.6 uses per-chunk VAO/VBO)
- [x] Mesh revision tracking and GL buffer refresh on world changes
- [x] Async block generation on a worker thread (implemented in B.6); mesh builds remain budgeted on the GL thread

**B.5 limitations at its original phase:** block generation and meshing were synchronous on the GL render thread. B.6 moves only block generation to a worker; CPU mesh building remains budgeted on the GL thread. A view distance of 4 requests 81 X/Z columns, each now represented by one 16 × 128 × 16 chunk. This prototype does not prove an infinite world, stable 60 FPS, or low-end device performance; profiling and device validation remain pending.


### B.6 — Async Chunk Generation

- [x] Canonical chunk dimensions 16 × 128 × 16 (32,768 block IDs)
- [x] Shared X-fastest block layout in VoxelDims.h and portable engine dimensions
- [x] One C++ worker thread for terrain block generation only
- [x] Pending request queue with coordinate deduplication and a bounded queue
- [x] Nearest-first queued work reprioritized around the current camera
- [x] Stale queued/ready requests pruned after camera movement
- [x] Ready queue collected on the GL/render thread (maximum 6 results per update)
- [x] CPU mesh build budget limited to 2 chunks per update
- [x] Neighbor meshes marked dirty when a chunk loads or unloads
- [x] GPU upload remains exclusively on the GLSurfaceView GL thread
- [x] Per-chunk VAO/VBO resources update only when that chunk mesh revision changes
- [x] Unloaded chunk GPU resources are released on the GL thread
- [x] Worker shutdown, queue cleanup, and join before WorldGenerator destruction
- [ ] Native CMake/CTest execution
- [ ] Android NDK/APK build and EGL runtime verification
- [ ] Device profiling during continuous chunk traversal

**B.6 limitations:** generation is asynchronous, but CPU meshing remains on the GL thread. Each changed chunk is uploaded independently, avoiding a full-world VBO refresh; this trades larger uploads for more draw calls (up to one per loaded chunk with a non-empty mesh). One worker is used; multi-worker scheduling, frustum culling, GPU instancing, and persistence are future work. A source-level implementation is not evidence of stable 60 FPS.

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
