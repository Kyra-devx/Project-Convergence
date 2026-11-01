# Game Design Document — Project Convergence

**Document version:** 0.1  
**Status:** Initial design baseline  
**Project type:** Independent voxel sandbox and technical prototype  
**Initial platform:** Android

## 1. Executive Summary

Project Convergence explores a voxel sandbox that combines deep, predictable simulation with a native runtime designed for constrained devices. It takes design lessons from Java and Bedrock ecosystems without copying proprietary code, assets, or content.

The core promise is a world whose rules are understandable, testable, and consistent. Rendering and simulation may be optimized independently, but optimization must not silently change gameplay outcomes.

## 2. Product Vision

Build an original voxel platform with deterministic automation, a responsive native runtime, scalable chunk processing, deliberate combat, meaningful survival risk, data-driven content, and documented extension points.

“Java depth, Bedrock speed” is a design aspiration, not a claim of parity or measured performance.

## 3. Design Pillars

### 3.1 Predictable simulation
Gameplay state changes must follow explicit rules. Tick ordering, neighbor updates, scheduled events, and random sources should be documented and tested.

### 3.2 Performance with measurable budgets
Use native code and parallelize independent work such as chunk generation and mesh building. Parallel execution must not make authoritative gameplay state nondeterministic.

### 3.3 Open content boundaries
World data, resource packs, and extension metadata should use documented, versioned formats. Third-party content is untrusted and must be sandboxed.

### 3.4 Mobile-first interaction
Touch controls, readability, safe areas, accessibility, thermal limits, and battery usage are first-class constraints.

## 4. Target Platform

The first target is Android with minimum SDK 24. The application layer uses Java and Android lifecycle APIs. The native runtime uses C++ through Android NDK and JNI. OpenGL ES 3.0 is the initial rendering baseline.

Vulkan may be evaluated later. Hardware ray tracing is not a baseline requirement and must not be assumed on mobile devices.

## 5. Core Gameplay

### 5.1 World and blocks

The world is composed of discrete blocks grouped into chunks. The model should support stable block identifiers, explicit coordinates and bounds, metadata separated from render data, versioned save serialization, and future stateful blocks.

The initial native library contains only a small bounded voxel data model. It is not yet a complete world, chunk manager, or persistence system.

### 5.2 Exploration and survival

The intended loop is exploration, resource gathering, construction, experimentation, and progression. Recipes, progression pacing, creature ecology, and economy remain open design work.

### 5.3 World generation

The long-term direction is a hybrid biome system with deterministic seed-based generation. Separate large-scale climate and biome selection from local terrain shaping, structures, and decoration.

A seed plus generation version must produce reproducible results. Algorithm changes require versioned compatibility behavior for existing worlds.

## 6. Redstone and Automation

### 6.1 Goal
Automation should behave predictably enough for timing-sensitive systems and repeatable developer tests.

### 6.2 Simulation model
The proposed design uses a fixed target of 20 ticks per second, an explicit event queue, documented event ordering, deterministic simulation randomness, and separation between authoritative state and visual interpolation.

Breadth-first propagation and Java-style update behavior are candidate semantics, not implemented guarantees. Exact update order must be specified against a named reference version before compatibility claims are made.

### 6.3 Behavioral compatibility
Potential reference behaviors include quasi-connectivity, observer timing, piston update order, directional power, and block event scheduling. Each behavior needs a written rule and regression test. Differences belong in a compatibility matrix.

### 6.4 Test strategy
Data-driven fixtures should include a stable case ID, reference edition/version, initial world state, ordered input events, expected transitions and tick numbers, and explicit tolerance rules.

The roadmap target is 700+ cases after the schema and harness are stable. This is a future goal, not the current test count.

## 7. Combat Design

The intended direction combines deliberate attack timing with readable defensive options and mobile-friendly input. Candidate mechanics include attack cooldown and damage scaling, shield/block actions, directional and sweeping attacks, clear hit feedback, and input buffering with explicit cancellation rules.

Exact values, animations, weapon categories, and balance tables are not defined in this version. Evaluate changes for accessibility and network fairness before release.

## 8. Hardcore and World Risk

A hardcore-style ruleset is planned as an optional world setting. Its rules must be selected at world creation and displayed before confirmation.

Possible constraints include permanent world death, restricted recovery, and stronger survival pressure. The final mode must explain irreversible consequences and recovery options. No hidden deletion of user data is permitted.

## 9. Compatibility and Modding

The project aims to provide open extension points and adapters for selected data formats. A future compatibility layer may parse datapack-style resources or map a documented subset of add-on behaviors.

Arbitrary Fabric mods are Java programs that may depend on JVM internals, mixins, loaders, and version-specific APIs. A general translator that runs all such mods directly in a native C++ runtime is not assumed feasible. Publish a supported subset, version matrix, and unsupported-feature report for any compatibility effort.

## 10. Multiplayer Direction

Cross-device multiplayer is a long-term goal. The authoritative simulation model, protocol, server model, anti-cheat approach, and version negotiation remain unspecified.

Networking requires explicit ownership of authoritative state and rules for latency, prediction, rollback, and reconciliation before public testing.

## 11. Accessibility and UX

Plan for remappable touch controls, adjustable UI scale, safe-area support, high-contrast cues, subtitles or visual cues for important audio events, color-independent indicators, and lifecycle-safe saving.

## 12. Non-Goals for v0.1

- Reproducing proprietary code, assets, textures, sounds, or branding.
- Claiming complete Java/Bedrock parity.
- Running arbitrary third-party Java mods inside the native runtime.
- Shipping a complete survival game or production multiplayer service.
- Claiming performance advantages without reproducible benchmarks.

## 13. Prototype Acceptance Criteria

1. Android project syncs and builds reproducibly with documented tool versions.
2. App starts on an Android 7.0+ compatible device or emulator.
3. OpenGL ES 3.0 context is created and a frame is presented.
4. JNI calls use documented threads and survive pause/resume.
5. Standalone voxel data model passes native unit tests.
6. Build, runtime, and test limitations are recorded.

## 14. Open Design Decisions

World vertical limits and chunk dimensions; block/item identifier scheme; save-file format and migration; exact redstone reference version and update order; combat timing; biome generation; multiplayer authority/protocol; and extension API/sandbox model.

Resolve each through a small design proposal with testable acceptance criteria.
