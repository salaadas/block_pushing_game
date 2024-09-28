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
- Tone mapping/gamma correction, HDR.
- Catalog assets system that supports hotloading.
- Serialization and metadata system for undo and save games.
- Custom allocators and data structures (dynamic arrays, hash tables, pools, bucket arrays, ...).
- Also, it is a Sokoban style game, with game logic and entity system.

