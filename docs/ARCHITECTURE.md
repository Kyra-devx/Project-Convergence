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
- native-lib.cpp: JNI boundary, GL shader setup, demo chunk mesh upload, input synchronization, delta-time loop, camera updates, and draw call.

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

The demo initializes the camera just outside the B.2 block structure and aims toward its center. Walking uses the horizontal projection of the front vector, so looking up/down does not cause the movement joystick to fly or sink the camera. Collision, gravity, jump, and terrain height handling remain future work.

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

The CPU-side mesher skips air, checks six axis-aligned neighbors, omits internal faces adjacent to another solid block in the same chunk, and emits two triangles for each exposed face. Vertices contain position, normal, and RGB color.

Faces at chunk boundaries remain visible because adjacent-chunk data is not available yet. Neighbor-aware culling is part of B.5. The demo contains one stepped sample chunk, not a terrain generator or world streamer.

## 9. Rendering Pipeline

Initial Android baseline: OpenGL ES 3.0.

1. Read vertex/fragment shader assets.
2. Compile and link shader program.
3. Build the demo chunk mesh and upload it using GL_STATIC_DRAW.
4. Bind position, normal, and color attributes into one VAO.
5. On each frame, update Camera using look delta and joystick state.
6. Calculate projection × view matrix and upload uMVP.
7. Issue one glDrawArrays(GL_TRIANGLES, ...) for the demo chunk.

Depth testing and back-face culling remain enabled. Face winding is checked by native tests. The current renderer uses one static demo chunk and one draw call; it does not yet include frustum culling, greedy meshing, transparent passes, or chunk batching.

## 10. Persistence, Compatibility, Security

Save files need a magic identifier and format version, world generation version and seed, chunk coordinates, block/metadata payloads, integrity checks, and migration rules.

Compatibility should use adapters for documented formats, normalize IDs/metadata, validate resource sizes and paths, report unsupported constructs, and restrict extension execution. Direct execution of arbitrary JVM mods from C++ is outside the current architecture.

Do not commit secrets, signing keys, local config, or personal data. Treat worlds, packs, and archives as untrusted. Reject path traversal and oversized decompressed content.

## 11. Build and Test Strategy

Portable engine: desktop CMake build and CTest for voxel bounds and chunk meshing. Existing tests cover empty, single, adjacent, solid chunk face counts and winding/normal agreement.

Android: Gradle app module, CMake native build, emulator/device smoke test, multi-touch test, pause/resume, and EGL surface recreation. Device verification is distinct from source-level implementation and must be recorded separately.

CI should pin tool versions, cache dependencies safely, run formatting/native tests, and build the Android APK. A green compile job is not a substitute for runtime testing on devices.

## 12. Decisions and Open Questions

Collision and gravity model; world vertical bounds; chunk dimensions; neighbor-chunk ownership; save encoding/compression; redstone tick/update ordering; multiplayer authority; extension API and sandbox; and minimum GPU feature set.

Record decisions with context, alternatives, consequences, and migration impact.
