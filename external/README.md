# external

This directory is reserved for third-party code that must be vendored into the repository.

Current project policy:
- Prefer `vcpkg.json` for standard dependencies.
- Keep code here only when a library must be built from source in-tree or needs local patching.
- Wire any vendored library into CMake explicitly when it becomes part of the build.
- Remove or relocate unused vendored code so the dependency strategy stays obvious.
