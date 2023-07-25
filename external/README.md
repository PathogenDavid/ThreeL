ThreeL External Dependencies
===============================================================================

Most of ThreeL's dependencies are incorporated into this repository directly for the sake of simplicity (a few others come from NuGet.)

They're taken from the following versions:

| Dependency | File/Directory | Upstream Version | Modified |
|------------|----------------|------------------|---------------|
| Dear ImGui | `DearImGui/` | [`docking a88e5be7f4`](https://github.com/ocornut/imgui/tree/a88e5be7f478233e74c72c72eabb1d5f1cb69bb5) | Yes
| ImGuizmo | `ImGuizmo.*` | [`822be7b44c`](https://github.com/CedricGuillemet/ImGuizmo/tree/822be7b44c37dbe98d328739ebe0d5a1ea87ecfc) | Yes
| JSON for Modern C++ | `json.hpp` | [`v3.10.4`](https://github.com/nlohmann/json/tree/fec56a1a16c6e1c1b1f4e116a20e79398282626c) |
| Minigraph Bitonic Sort | `BitonicSort/` | [`a79e01c4c3`](https://github.com/microsoft/DirectX-Graphics-Samples/tree/a79e01c4c39e6d40f4b078688ff95814d166d34f) | Yes
| Sponza | `Assets/Sponza/` | [`189f80d7d4`](https://github.com/KhronosGroup/glTF-Sample-Models/tree/189f80d7d44f76d8f9be8e337d4c6cb85ef521a4) |
| stb_image | `stb_image.h` | [`TinyGLTF fork v2.8.9`](https://github.com/syoyo/tinygltf/tree/350c2968025882bdf823e7892d02328548b46435) |
| TinyGLTF | `tiny_gltf.h` | [`v2.8.9`](https://github.com/syoyo/tinygltf/tree/350c2968025882bdf823e7892d02328548b46435) |
| xxHash | `xxhash.*` | [`0656ed7539`](https://github.com/Cyan4973/xxHash/tree/0656ed753994ce3d04a39ca132242e98fddef136) |

See [the third-party notice listing](../THIRD-PARTY-NOTICES.md) for details on licenses governing these dependencies.

## Changes made

### Dear ImGui

* Added names to Direct3D 12 objects created by the backend

### ImGuizmo

* Fixed reverse Z infinite far plane perspective matricies breaking `ComputeCameraRay` ([Thanks Thomas!](https://github.com/CedricGuillemet/ImGuizmo/pull/210))
* Fixed left-handed reverse Z infinite far plane matricies breaking behind-camera check

### Minigraph Bitonic Sort

* Renamed files
* Fixed some compiler warnings in `BitonicPrepareIndirectArgs.cs.hlsl`
