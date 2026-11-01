# Redstone Test Suite

This directory will hold data-driven regression fixtures for the Project Convergence simulation.

Each case should identify a stable ID, reference edition and exact version, initial world state, ordered input events, expected state transitions and tick numbers, and expected final state.

Rules:
- Do not describe a case as parity-tested until the reference behavior is recorded and the test is executable.
- Keep fixtures small and reproducible.
- Record known differences rather than silently weakening assertions.
- Include boundary, update-order, and repeated-trigger cases.

The test harness and fixture schema are not implemented yet. The roadmap target of 700+ cases is planned work, not the current suite size.
