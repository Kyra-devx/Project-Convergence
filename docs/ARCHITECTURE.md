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
┌────────────────────────────────────────────────────┐
│ Android Activity + TouchControlsOverlay             │
│ Left: virtual joystick · Right: camera look         │
└──────────────────────┬─────────────────────────────┘
                       │ multi-touch MotionEvent
┌──────────────────────▼─────────────────────────────┐
│ VoxelGLSurfaceView + VoxelRenderer                  │
│ GLSurfaceView.Renderer · frame clock · EGL          │
└──────────────────────┬─────────────────────────────┘
                       │ JNI input/render calls
┌──────────────────────▼─────────────────────────────┐
│ Mutex-protected input snapshot · native Camera     │
│ C++ Chunk mesher · delta-time movement · GLM        │
└──────────────────────┬─────────────────────────────┘
                       │ MVP + VBO
┌──────────────────────▼─────────────────────────────┐
│ OpenGL ES 3.0: GLSL · VAO/VBO · Depth/Culling       │
└────────────────────────────────────────────────────┘
~~~~

## 3. Repository Modules

### app/

Android entry point, lifecycle, platform UI, touch input, and GL surface.

### app/src/main/java/com/convergence/voxel/

- MainActivity creates the game surface and places touch controls above it in a FrameLayout.
- VoxelGLSurfaceView owns the GLSurfaceView and tracks one left joystick pointer and one right look pointer. The two can be active simultaneously.
- VoxelRenderer owns GL renderer lifecycle and calls native init, resize, render, and input functions.
- TouchControlsOverlay draws the floating joystick base/knob and forwards MotionEvents to the surface input controller.

The overlay is a transparent Android View above the GLSurfaceView. It captures the touch stream so the joystick and camera-look gestures are routed consistently.

### app/src/main/cpp/

- Camera.h / Camera.cpp: native camera position, yaw, pitch, front vector, look processing, movement, view matrix, and perspective matrix.
- native-lib.cpp: JNI boundary, GL shader setup, generated terrain mesh upload, input synchronization, delta-time loop, camera updates, and draw call.
- VoxelDims.h: canonical 16 × 128 × 16 chunk dimensions and contiguous block indexing.
- WorldGenerator.h / WorldGenerator.cpp: seeded height/detail noise, complete 128-block chunk data, surface layers, and terrain-height sampling.
- ChunkManager.h / ChunkManager.cpp: signed X/Z chunk coordinates, worker lifecycle, bounded pending/ready queues, incremental loading/unloading, neighbor lookup, mesh budget, and merged CPU geometry.

### app/src/main/assets/shaders/

GLSL ES 3.0 vertex and fragment shaders. Position, normal, and RGB color use attribute locations 0, 1, and 2.

### engine/

Portable C++ library with public headers in include/, implementations in src/, native tests in tests/, and CMake build. Chunk is the CPU mesh-generation unit; B.6 uses 16 × 128 × 16 storage.

## 4. Camera

Camera uses GLM:

- position: world-space camera location.
- yaw: horizontal angle in degrees, initially -90° to face -Z.
- pitch: vertical angle in degrees, clamped to [-89°, 89°].
- movementSpeed: world units per second.
- lookSensitivity: degrees per input pixel.
- fov: perspective vertical field of view in degrees.

getFrontVector converts yaw/pitch to a normalized direction. getViewMatrix calls glm::lookAt. getProjectionMatrix(aspect) calls glm::perspective with near/far planes 0.1 and 500 world units.

The demo initializes the camera above the generated terrain. Walking uses the horizontal projection of the front vector, so looking up/down does not cause the movement joystick to fly or sink the camera. Gravity advances a vertical velocity each frame in Camera; nativeRender samples WorldGenerator height on the GL thread and applies the floor clamp at 1.7 world units above the surface. Jumping remains future work.

GLM is a header-only C++ dependency fetched by CMake from the exact upstream commit corresponding to release 1.0.1. The initial Android native configure requires network access unless the dependency has already been cached.

## 5. JNI Input Boundary and Threading

Touch events originate on Android's UI thread. nativeHandleLook accumulates relative look deltas; nativeHandleJoystick updates normalized forward/strafe values.

A mutex protects the pending input state. On each GL frame, nativeRender takes a snapshot, clears only the accumulated look deltas, and retains joystick values until release. This prevents UI-thread writes from racing with the GL thread's camera update.

All OpenGL calls remain on the thread that owns the current GLSurfaceView EGL context. Native camera updates and rendering also run on that GL thread.

B.6 adds one C++ worker thread owned by ChunkManager. The worker reads the immutable WorldGenerator configuration and writes only a private ChunkData block array. It does not access the chunk map, camera, mesh vertices, JNI input, or OpenGL. Completed data is placed in a mutex-protected ready queue. The GL thread collects at most 6 results per update, inserts them into the chunk map, and marks the new chunk plus loaded cardinal neighbors dirty. A bounded dirty queue builds at most 2 CPU meshes per update. The worker is stopped, notified, and joined before ChunkManager is destroyed and before WorldGenerator is released.

## 6. Game Loop and Delta Time

VoxelRenderer.onDrawFrame passes Android uptime in milliseconds to nativeRender.

The native loop calculates elapsed seconds from the previous frame. The delta is clamped to [0, 0.1] seconds to prevent large movement jumps after stalls or lifecycle transitions. nativeResetFrameClock resets the atomic frame timestamp during Activity pause/resume.

Movement is multiplied by speed and delta time, so speed is frame-rate independent. Diagonal movement is normalized to avoid moving faster than straight-line joystick input.

## 7. Touch Controls

The screen is divided into two input zones:

- **Left half:** first eligible pointer owns the floating joystick. Its origin is set at pointer-down; the knob is clamped to a density-scaled radius. The dead zone suppresses accidental drift. Up maps to forward, down to backward, and horizontal offset to strafe.
- **Right half:** first eligible pointer owns camera look. Pointer movement is sent as relative X/Y deltas.

One pointer per zone is tracked by pointer ID, allowing simultaneous movement and look. Additional pointers in an already-owned zone are ignored. Pointer-up releases only the matching control; ACTION_CANCEL and Activity pause reset all movement values.

The overlay visual is a simple touch indicator, not an interactive HUD system. Sensitivity settings, gesture remapping, button actions, and accessibility options remain future work.

## 8. World Data, Chunks, and Mesh Generation

Chunk stores 16 × 128 × 16 block IDs (32,768 bytes) using the shared X-fastest layout in VoxelDims.h. Block ID 0 is air; the prototype also defines grass (1), dirt (2), and stone (3).

The CPU-side mesher skips air, checks six axis-aligned neighbors, omits internal faces adjacent to another solid block, and emits two triangles for each exposed face. An optional neighbor lookup supplies blocks outside the current 16 × 128 × 16 chunk, allowing culling across horizontal chunk boundaries. Vertices contain position, normal, and RGB color.

When a neighboring column is not loaded, its blocks are treated as air; when it becomes available or is unloaded, the affected columns are remeshed. WorldGenerator fills each 16 × 128 × 16 column with stone, dirt, and grass layers. ChunkManager loads the nearest missing X/Z columns incrementally and unloads columns outside the view-distance-plus-hysteresis threshold. A view distance of 4 requests 81 columns; the raw block payload for those columns is 2,654,208 bytes (about 2.53 MiB), excluding mesh and container overhead.

## 9. Rendering Pipeline

Initial Android baseline: OpenGL ES 3.0.

1. Read vertex/fragment shader assets.
2. Compile and link shader program.
3. Initialize WorldGenerator, ChunkManager, and its single CPU-only worker thread.
4. The worker generates block arrays for the nearest requested 16 × 128 × 16 columns; it never calls OpenGL.
5. On the GL thread, collect up to 6 completed arrays and build up to 2 dirty chunk meshes per update with neighbor-aware culling.
6. Keep one VAO/VBO per loaded chunk. Upload only when that chunk’s mesh revision changes, using GL_DYNAMIC_DRAW.
7. Update Camera and ChunkManager, calculate projection × view, apply the chunk X/Z model transform, upload uMVP, and draw each ready chunk on the GL thread.

Depth testing and back-face culling remain enabled. Face winding is checked by native tests. ChunkManager maintains CPU meshes per loaded column. The renderer owns a matching VAO/VBO per chunk and uploads only a chunk whose mesh revision changed; unloaded chunk GPU resources are deleted on the GL thread. Draw calls are issued per chunk with an X/Z model translation. Terrain block generation runs on one worker; mesh building (budget 2/update) and GPU upload remain on the GL thread. This avoids repeated full-world buffer uploads but increases draw-call count with the number of loaded chunks. Frustum culling, greedy meshing, transparent passes, and level-of-detail selection are not implemented.

## 10. Persistence, Compatibility, Security

Save files need a magic identifier and format version, world generation version and seed, chunk coordinates, block/metadata payloads, integrity checks, and migration rules.

Compatibility should use adapters for documented formats, normalize IDs/metadata, validate resource sizes and paths, report unsupported constructs, and restrict extension execution. Direct execution of arbitrary JVM mods from C++ is outside the current architecture.

Do not commit secrets, signing keys, local config, or personal data. Treat worlds, packs, and archives as untrusted. Reject path traversal and oversized decompressed content.

## 11. Build and Test Strategy

Portable CMake/CTest: voxel bounds, full-height chunk meshing, seeded 16 × 128 × 16 generation, and asynchronous ChunkManager loading/unloading. Tests cover deterministic block data, seed variation, surface layers, negative chunk coordinates, request completion, dirty-mesh draining, neighbor culling, and worker shutdown. These tests are added but remain NOT_RUN until executed in a suitable build environment.

Android: Gradle app module, CMake native build, emulator/device smoke test, multi-touch test, pause/resume, EGL surface recreation, and profiling while the 81-column view is populated and the player crosses chunk boundaries. Device verification is distinct from source-level implementation and must be recorded separately.

CI should pin tool versions, cache dependencies safely, run formatting/native tests, and build the Android APK. A green compile job is not a substitute for runtime testing on devices.

## 12. Decisions and Open Questions

Collision and gravity model; world vertical bounds; chunk dimensions; neighbor-chunk ownership; save encoding/compression; redstone tick/update ordering; multiplayer authority; extension API and sandbox; and minimum GPU feature set.

Record decisions with context, alternatives, consequences, and migration impact.
