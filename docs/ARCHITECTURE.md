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
                       │ Java calls / AssetManager
┌──────────────────────▼──────────────────────────┐
│ JNI: Asset loading · Surface lifecycle           │
└──────────────────────┬──────────────────────────┘
                       │ Native API
┌──────────────────────▼──────────────────────────┐
│ C++: Chunk data · Face-culling mesher            │
│ World · Simulation · Tick · Redstone             │
└──────────────────────┬──────────────────────────┘
                       │ Vertex buffer
┌──────────────────────▼──────────────────────────┐
│ OpenGL ES 3.0: Shader · VAO/VBO · Depth/Culling  │
└─────────────────────────────────────────────────┘
~~~~

The Android path now contains a demo chunk render path and the portable engine contains a CPU mesher. First-person input, general world simulation, streaming, and production rendering features remain future work.

## 3. Repository Modules

### app/

Android entry point: Activity lifecycle, UI, touch input, platform permissions, JNI declarations, native library loading, and GL surface lifecycle. The Activity must not own simulation rules or perform expensive world operations on the UI thread.

### app/src/main/cpp/

Android-specific native bridge and renderer integration. It loads GLSL files from Android assets, compiles/links the shader program, uploads the demo mesh, creates the MVP camera matrix, and issues the OpenGL ES draw call. GL objects must be recreated after EGL context recreation.

### app/src/main/assets/shaders/

GLSL ES 3.0 vertex and fragment shaders. The vertex layout uses position at location 0, normal at location 1, and RGB color at location 2. The fragment shader applies a directional diffuse term with an ambient minimum.

### engine/

Portable C++ library with public headers in include/, implementations in src/, native tests in tests/, and a CMake build. VoxelWorld is a separate fixed-width data model; Chunk is the 16³ mesh-generation unit used by the B.2 prototype.

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

MainActivity passes its AssetManager to the GL thread's surface-created callback. Native code uses AAssetManager_fromJava to read shader assets; shader compilation and all GL calls occur while the GLSurfaceView context is current.

## 5. World Data, Chunks, and Mesh Generation

Chunk stores 16 × 16 × 16 block IDs. Block ID 0 is air; the prototype also defines grass (1), dirt (2), and stone (3). Coordinates outside the chunk read as air.

The CPU-side mesher:

1. Skips air blocks.
2. Examines six axis-aligned neighbors for each non-air block.
3. Omits a face when its neighbor inside the same chunk is non-air.
4. Emits two triangles (six vertices) for each exposed face.
5. Writes position, face normal, and RGB color into each Vertex.

A face at the chunk boundary is retained because adjacent-chunk data is not available yet. When chunk streaming is implemented, a mesher should consult neighbor data so a solid block in an adjacent chunk can occlude the boundary face. Until then, adjacent chunks may generate duplicate internal boundary faces.

The 16³ chunk is a rendering prototype, not the final world storage format. Chunk dimensions and vertical world bounds remain design decisions.

For a fully solid 16³ chunk, naive meshing emits 16³ × 6 × 2 = 49,152 triangles. This mesher emits only the six outer surfaces: 6 × 16² faces × 2 triangles = 3,072 triangles. Terrain savings depend on geometry and occupancy; no fixed percentage is guaranteed.

## 6. Simulation and Determinism

Use a fixed logical tick. Rendering may interpolate but must not mutate authoritative gameplay state.

A future scheduler must define event time and priority, stable tie-breaking, neighbor update order, cancellation/deduplication, overload handling, and deterministic random streams.

Workers may generate chunks or meshes from immutable snapshots. Return world/chunk revisions so stale jobs can be discarded. Authoritative block updates remain ordered according to the documented simulation model.

## 7. Rendering Pipeline

Initial Android baseline: OpenGL ES 3.0.

The B.2 demo path:

1. Read shaders/vertex_shader.glsl and shaders/fragment_shader.glsl from Android assets.
2. Compile and link the shader program.
3. Generate a stepped block-stack sample (stone, dirt, grass) in a single 16³ chunk.
4. Build its CPU mesh using face culling.
5. Upload the interleaved Vertex array to a VBO with GL_STATIC_DRAW.
6. Configure one VAO: position (location 0), normal (location 1), and color (location 2).
7. Calculate a fixed perspective/look-at MVP matrix and issue one glDrawArrays(GL_TRIANGLES, ...) call for the demo chunk.

The shader uses normalized face normals and a directional light with an ambient floor. Depth testing and back-face culling are enabled. Face winding is tested against declared normals to prevent outward faces from being culled accidentally.

This is a static demo camera, not the first-person camera or touch-control system in B.3. Transparent geometry, texture atlases, multiple material passes, frustum culling, greedy meshing, occlusion, and multi-chunk batching are future tasks. Benchmark low-, mid-, and high-tier devices before selecting optimization priorities. Add Vulkan only behind an abstraction and only if measured benefits justify complexity.

## 8. Threading Model

| Work | Thread ownership |
|---|---|
| Android UI and lifecycle | Main/UI thread |
| OpenGL ES calls | Thread owning current GL context |
| Authoritative simulation | Single deterministic owner initially |
| Chunk generation | Worker pool with immutable inputs |
| CPU mesh generation | Worker pool with immutable snapshots in a future phase |
| GPU upload/draw | GL thread with current context |
| File I/O | Dedicated I/O task or worker |

No background task should assume access to an Activity or live GL context. Define cancellation and shutdown for each worker. The B.2 demo currently builds and uploads one mesh during surface initialization; asynchronous mesh scheduling is deferred.

## 9. Persistence

Save files need a magic identifier and format version, world generation version and seed, chunk coordinates, block/metadata payloads, integrity checks, and migration rules. Writes should be atomic where possible. Failed saves must not silently overwrite the last known-good world.

## 10. Compatibility Layer

Use adapters that parse documented formats, normalize IDs/metadata, validate resource sizes and paths, report unsupported constructs, and restrict extension execution.

Direct execution of arbitrary JVM mods from C++ is outside the current architecture. A Java compatibility host, if ever considered, is a separate runtime with its own security and performance review.

## 11. Security and Privacy

Do not commit secrets, signing keys, local config, or personal data. Treat worlds, packs, and archives as untrusted. Reject path traversal and oversized decompressed content. Avoid unrestricted scripts. Keep logs free of personal data and private world content unless explicitly opted in.

Repository policy documents under .github/ remain authoritative for contribution and audit expectations.

## 12. Build and Test Strategy

Android: Gradle app module, CMake native build, device/emulator smoke test, pause/resume and surface-recreation tests. Android build and device runtime validation must be recorded separately from source-level implementation.

Portable engine: desktop CMake build, unit tests for bounds/indexing and chunk meshing, sanitizer runs where supported, and deterministic fixtures once simulation exists.

The chunk_mesher_test validates empty, single-block, adjacent-block, solid-chunk face counts, and winding/normal agreement for all six face directions. CTest runs it alongside voxel_world_test.

CI should pin tool versions, cache safely, run formatting/native tests, and build the Android APK. A green compile job is not a substitute for runtime testing on devices.

## 13. Decisions and Open Questions

Chunk dimensions and vertical bounds; neighbor-chunk data ownership; rendering abstraction; save encoding/compression; tick/update ordering; lighting; network authority; scripting sandbox; and minimum GPU feature set.

Record decisions with context, alternatives, consequences, and migration impact.
