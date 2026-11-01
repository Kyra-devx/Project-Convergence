# Technical Architecture — Project Convergence

**Status:** Initial architecture baseline  
**Target platform:** Android first  
**Native language:** C++17  
**Android language:** Java 17  
**Renderer baseline:** OpenGL ES 3.0

## 1. Goals

Separate platform lifecycle, native simulation, world data, rendering, and compatibility code. Keep gameplay authoritative and deterministic, isolate Android APIs from portable engine code, move CPU-heavy work off the UI thread, version world/content formats, treat imported content as untrusted, and measure before optimizing.

## 2. System Overview

~~~~text
┌─────────────────────────────────────────────────┐
│ Android App: Activity · Touch · Lifecycle        │
└──────────────────────┬──────────────────────────┘
                       │ Java calls
┌──────────────────────▼──────────────────────────┐
│ JNI: Validation · Thread affinity · Ownership   │
└──────────────────────┬──────────────────────────┘
                       │ Native API
┌──────────────────────▼──────────────────────────┐
│ C++: World · Simulation · Tick · Redstone       │
│ Combat · Persistence · Chunk processing         │
└──────────────────────┬──────────────────────────┘
                       │ Render commands / buffers
┌──────────────────────▼──────────────────────────┐
│ Rendering: OpenGL ES 3.0 baseline               │
│ Vulkan is a possible later backend              │
└─────────────────────────────────────────────────┘
~~~~

The repository currently contains a minimal Android surface/JNI scaffold and a standalone voxel data library. Most layers above are target boundaries, not completed systems.

## 3. Repository Modules

### app/
Android entry point: Activity lifecycle, UI, touch input, platform permissions, JNI declarations, native library loading, and GL surface lifecycle. The Activity must not own simulation rules or perform expensive world operations on the UI thread.

### app/src/main/cpp/
Android-specific native bridge and platform renderer integration. The initial shared library only configures the viewport and clears the GL frame.

### engine/
Portable C++ library with public headers in include/, implementations in src/, native tests in tests/, and a CMake build. The initial VoxelWorld is a fixed-size data-model example, not a chunk manager or full simulation.

### tools/
redstone-test-suite/ stores planned behavioral fixtures. world-converter/ documents future format mapping and import/export tooling. Conversion must report unsupported or lossy data.

## 4. JNI Boundary

JNI is a platform boundary, not a general-purpose data bus.

1. Define ownership for every native allocation crossing the boundary.
2. Validate dimensions, lengths, and nullable inputs.
3. Keep GL calls on the thread with the current GL context.
4. Keep Android UI calls on the main thread.
5. Avoid retaining JNI references longer than required.
6. Expose coarse-grained operations rather than crossing JNI for every block/vertex.
7. Handle pause/resume and GL context recreation explicitly.

Current methods are surface lifecycle callbacks and do not pass world buffers.

## 5. World Data and Chunking

Use chunk-local coordinates and stable identifiers. Chunk dimensions remain a design decision.

Separate logical block state, metadata/block entities, lighting and derived state, render meshes/GPU resources, and persistence representation. Derived render data must be rebuildable from authoritative state. GPU handles must never be serialized.

## 6. Simulation and Determinism

Use a fixed logical tick. Rendering may interpolate but must not mutate authoritative gameplay state.

A future scheduler must define event time and priority, stable tie-breaking, neighbor update order, cancellation/deduplication, overload handling, and deterministic random streams.

Workers may generate chunks or meshes from immutable snapshots. Return world/chunk revisions so stale jobs can be discarded. Authoritative block updates remain ordered according to the documented simulation model.

## 7. Rendering Pipeline

Initial Android baseline: OpenGL ES 3.0.

Planned stages: collect visible chunks; build/retrieve meshes; upload buffers; render opaque geometry; render cutout/transparent geometry with explicit ordering; render UI and diagnostics.

Face culling, greedy meshing, frustum culling, occlusion, and GPU buffer reuse are future tasks. Benchmark low-, mid-, and high-tier devices. Add Vulkan only behind an abstraction and only if measured benefits justify complexity.

## 8. Threading Model

| Work | Thread ownership |
|---|---|
| Android UI and lifecycle | Main/UI thread |
| OpenGL ES calls | Thread owning current GL context |
| Authoritative simulation | Single deterministic owner initially |
| Chunk generation | Worker pool with immutable inputs |
| Mesh generation | Worker pool with immutable snapshots |
| File I/O | Dedicated I/O task or worker |

No background task should assume access to an Activity or live GL context. Define cancellation and shutdown for each worker.

## 9. Persistence

Save files need a magic identifier and format version, world generation version and seed, chunk coordinates, block/metadata payloads, integrity checks, and migration rules. Writes should be atomic where possible. Failed saves must not silently overwrite the last known-good world.

## 10. Compatibility Layer

Use adapters that parse documented formats, normalize IDs/metadata, validate resource sizes and paths, report unsupported constructs, and restrict extension execution.

Direct execution of arbitrary JVM mods from C++ is outside the current architecture. A Java compatibility host, if ever considered, is a separate runtime with its own security and performance review.

## 11. Security and Privacy

Do not commit secrets, signing keys, local config, or personal data. Treat worlds, packs, and archives as untrusted. Reject path traversal and oversized decompressed content. Avoid unrestricted scripts. Keep logs free of personal data and private world content unless explicitly opted in.

Repository policy documents under .github/ remain authoritative for contribution and audit expectations.

## 12. Build and Test Strategy

Android: Gradle app module, CMake native build, device/emulator smoke test, pause/resume and surface-recreation tests.

Portable engine: desktop CMake build, unit tests for bounds/indexing, sanitizer runs where supported, and deterministic fixtures once simulation exists.

CI should pin tool versions, cache safely, run formatting/native tests, and build the Android APK. A green compile job is not a substitute for runtime testing on devices.

## 13. Decisions and Open Questions

Chunk dimensions and vertical bounds; rendering abstraction; save encoding/compression; tick/update ordering; lighting; network authority; scripting sandbox; and minimum GPU feature set.

Record decisions with context, alternatives, consequences, and migration impact.
