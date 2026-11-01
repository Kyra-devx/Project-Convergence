# World Converter

This directory is reserved for import/export utilities. No converter is implemented yet.

Before code is added, define:

1. Supported source and destination format versions.
2. Block and entity identifier mapping.
3. Coordinate and dimension mapping.
4. Metadata and block-entity handling.
5. Lossy conversion rules and machine-readable warnings.
6. Validation, archive-size limits, and path-traversal protection.

A conversion must never report full success when data was skipped or transformed without disclosure. Keep original input files unchanged and write converted output to a separate destination.
