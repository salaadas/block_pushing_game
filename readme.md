# Untitled block pushing game

![nexus shot 1](./screenshots/nexus_1.png)

This engine is in OpenGL and heavily relies on the Linux environment. I'm using Ubuntu 24.04 LTS to develop it.

The engine features:

- Shadow mapping (PCF, CSM is in progress).
- Skeletal animation.
- Animation HUD, Animation graph for minute controls.
- glTF (and FBX) loading.
- PBR material system.
- Immediate-mode GUI library (has buttons, text inputs, color picker, checkboxes, comboboxes, window panels, slidable regions, scrollable regions, ...).
- Font rendering with FreeType.
- Audio mixer with miniaudio, supports for .wav (drwav), .ogg (stb_vorbis), .mp3 (drmp3).
- Tone mapping/gamma correction, HDR.
- Parser that allows for include directive and some extra tags for GLSL shaders.
- Catalog assets system that supports hotloading.
- Serialization and metadata system for undo and save games.
- Custom allocators and data structures (dynamic arrays, hash tables, pools, bucket arrays, ...).
- Also, it is a Sokoban style game, with game logic and entity system.

