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
- WorldGenerator.h / WorldGenerator.cpp: seeded height/detail noise, vertical chunk section data, surface layers, and terrain-height sampling.
- ChunkManager.h / ChunkManager.cpp: signed X/Z chunk coordinates, incremental loading/unloading, vertical section ownership, neighbor lookup, mesh invalidation, and merged CPU geometry.

### app/src/main/assets/shaders/

GLSL ES 3.0 vertex and fragment shaders. Position, normal, and RGB color use attribute locations 0, 1, and 2.

### engine/

Portable C++ library with public headers in include/, implementations in src/, native tests in tests/, and CMake build. Chunk is the 16³ CPU mesh-generation unit used by the B.2/B.3 demo.

## 4. Camera

Camera uses GLM:

- position: world-space camera location.
- yaw: horizontal angle in degrees, initially -90° to face -Z.
- pitch: vertical angle in degrees, clamped to [-89°, 89°].
- movementSpeed: world units per second.
- lookSensitivity: degrees per input pixel.
- fov: perspective vertical field of view in degrees.

getFrontVector converts yaw/pitch to a normalized direction. getViewMatrix calls glm::lookAt. getProjectionMatrix(aspect) calls glm::perspective with near/far planes 0.1 and 500 world units.

The demo initializes the camera above the generated terrain. Walking uses the horizontal projection of the front vector, so looking up/down does not cause the movement joystick to fly or sink the camera. Gravity advances a vertical velocity each frame; floor collision samples the same deterministic height function used by world generation and clamps the eye to 1.7 world units above the surface. Jumping remains future work.

GLM is a header-only C++ dependency fetched by CMake from the exact upstream commit corresponding to release 1.0.1. The initial Android native configure requires network access unless the dependency has already been cached.

## 5. JNI Input Boundary and Threading

Touch events originate on Android's UI thread. nativeHandleLook accumulates relative look deltas; nativeHandleJoystick updates normalized forward/strafe values.

A mutex protects the pending input state. On each GL frame, nativeRender takes a snapshot, clears only the accumulated look deltas, and retains joystick values until release. This prevents UI-thread writes from racing with the GL thread's camera update.

All OpenGL calls remain on the thread that owns the current GLSurfaceView EGL context. Native camera updates and rendering also run on that GL thread.

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

Chunk stores 16 × 16 × 16 block IDs. Block ID 0 is air; the prototype also defines grass (1), dirt (2), and stone (3).

The CPU-side mesher skips air, checks six axis-aligned neighbors, omits internal faces adjacent to another solid block, and emits two triangles for each exposed face. An optional neighbor lookup supplies blocks outside the current 16³ section, allowing culling across horizontal chunk boundaries and between vertical terrain sections. Vertices contain position, normal, and RGB color.

When a neighboring column is not loaded, its blocks are treated as air; when it becomes available or is unloaded, the affected columns are remeshed. B.4 uses FastNoiseLite heightmap and detail noise, filling five vertical 16³ sections per X/Z column with stone, dirt, and grass layers. B.5 loads nearest missing X/Z columns incrementally and unloads columns outside the view-distance-plus-hysteresis threshold. A view distance of 4 targets 81 columns (405 cubic sections).

## 9. Rendering Pipeline

Initial Android baseline: OpenGL ES 3.0.

1. Read vertex/fragment shader assets.
2. Compile and link shader program.
3. Initialize WorldGenerator and ChunkManager; load nearby X/Z columns incrementally.
4. Build CPU meshes with neighbor-aware face culling across X/Z and vertical section boundaries.
5. Bind position, normal, and color attributes into one reusable VAO/VBO.
6. On each frame, update Camera and ChunkManager; refresh the merged GPU buffer with GL_DYNAMIC_DRAW only when the mesh revision changes.
7. Calculate projection × view matrix, upload uMVP, and draw the merged loaded terrain in one call.

Depth testing and back-face culling remain enabled. Face winding is checked by native tests. ChunkManager maintains per-column CPU meshes and combines them only when its mesh revision changes. The renderer uploads the merged mesh to a reusable VAO/VBO using GL_DYNAMIC_DRAW and issues one draw call. It does not yet include worker-thread generation, frustum culling, greedy meshing, transparent passes, or level-of-detail selection.

## 10. Persistence, Compatibility, Security

Save files need a magic identifier and format version, world generation version and seed, chunk coordinates, block/metadata payloads, integrity checks, and migration rules.

Compatibility should use adapters for documented formats, normalize IDs/metadata, validate resource sizes and paths, report unsupported constructs, and restrict extension execution. Direct execution of arbitrary JVM mods from C++ is outside the current architecture.

Do not commit secrets, signing keys, local config, or personal data. Treat worlds, packs, and archives as untrusted. Reject path traversal and oversized decompressed content.

## 11. Build and Test Strategy

Portable CMake/CTest: voxel bounds, chunk meshing, and seeded world generation. World-generation tests cover same-seed determinism, different-seed variation, surface material layers, vertical section placement, and air above terrain.

Android: Gradle app module, CMake native build, emulator/device smoke test, multi-touch test, pause/resume, EGL surface recreation, and profiling while the 81-column view is populated and the player crosses chunk boundaries. Device verification is distinct from source-level implementation and must be recorded separately.

CI should pin tool versions, cache dependencies safely, run formatting/native tests, and build the Android APK. A green compile job is not a substitute for runtime testing on devices.

## 12. Decisions and Open Questions

Collision and gravity model; world vertical bounds; chunk dimensions; neighbor-chunk ownership; save encoding/compression; redstone tick/update ordering; multiplayer authority; extension API and sandbox; and minimum GPU feature set.

Record decisions with context, alternatives, consequences, and migration impact.
