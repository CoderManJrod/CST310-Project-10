# Project 10 — Advanced Mapping Shaders

**Course:** CST-310 — Computer Graphics 
**School:** Grand Canyon University
**Authors:** Jared Walker & James Hohn
**Instructor:** Ricardo Citro
**Due Date:** April 17, 2026

---

## Overview

Project 10 is the second half of the two-part shader project in CST-310, building on the scene created in Project 9. The scene includes a magenta and white checkerboard floor with three objects standing on it. Each object uses a different mapping technique implemented in GLSL:

| Object   | Position | Mapping Technique     | Asset File          |
|----------|----------|-----------------------|---------------------|
| Sphere   | Left     | Diffuse texture + Blinn-Phong | Bump-Picture.jpg   |
| Cube     | Center   | Environment (cube map) reflection | 6 Humus Yokohama faces |
| Cylinder | Right    | Bump mapping from height map | Bump-Map.jpg |

A free-look fly-by camera from Project 9 is reused so the user can navigate around each object and see the mapping effects change with viewing angle.

---

## Files in This Repository

| File                    | Purpose                                           |
|-------------------------|---------------------------------------------------|
| `Proj10FinalSub.cpp`    | Complete, commented C++ source                    |
| `instructions.txt`      | Build instructions, dependencies, controls       |
| `CST-310_Project_10.docx` | Full project documentation                     |
| `README.md`             | This file                                         |
| `screenshot.png`        | Screenshot of the running scene                  |
| `Bump-Map.jpg`          | Grayscale height map for cylinder bump mapping    |
| `Bump-Picture.jpg`      | Stone brick diffuse texture for sphere           |
| `posx.jpg` ... `negz.jpg` | Six cube map faces (Humus Yokohama)            |

---

## Software Requirements

- Ubuntu 22.04 LTS or later
- g++ with C++17 support
- OpenGL 2.1 or higher with GLSL 1.20 shader support
- GLFW (`libglfw3-dev`)
- GLEW (`libglew-dev`)
- GLM (`libglm-dev`)
- SOIL (`libsoil-dev`) for texture and cube map loading
- assimp (`libassimp-dev`) included to match class compile convention

### Install Dependencies

```bash
sudo apt install build-essential libglfw3-dev libglew-dev \
                 libglm-dev libsoil-dev libassimp-dev -y
```

---

## Build and Run

```bash
g++ Proj10FinalSub.cpp -o run -lglfw -lGL -lGLEW -lSOIL -lassimp
./run
```

All asset files listed above must be in the same directory as the executable.

---

## Controls

| Key                | Action                                      |
|--------------------|---------------------------------------------|
| Arrow Left / Right | Yaw camera left / right                     |
| Arrow Up / Down    | Pitch camera up / down                      |
| Shift + Up / Down  | Move forward / backward along Z             |
| W / S              | Raise / lower camera eye height             |
| A / D              | Strafe left / right                         |
| Q / E              | Roll camera counter-clockwise / clockwise   |
| Ctrl + Arrows      | Fine-grained yaw / pitch                    |
| R                  | Reset camera to default position            |
| ESC                | Quit program                                |

---

## Shader Details

### Sphere — Diffuse Texture
The sphere samples `Bump-Picture.jpg` as its surface color using spherical UV coordinates, then applies standard Blinn-Phong lighting using the geometric surface normal.

### Cube — Environment Mapping
The cube uses the GLSL `reflect(I, N)` function to compute a reflection vector from the view ray and surface normal, then samples the Yokohama cube map along that direction. The result looks like a mirrored cube reflecting the surrounding harbor environment.

### Cylinder — Bump Mapping
The cylinder samples `Bump-Map.jpg` at the current UV and at small offsets in U and V to estimate the local surface gradient. That gradient becomes a perturbed normal in tangent space, transformed to world space through a TBN matrix, and used for lighting. The effect creates the illusion of surface relief without adding any extra geometry.

---

## References

- Blinn, J. F. (1978). Simulation of wrinkled surfaces. *ACM SIGGRAPH Computer Graphics, 12*(3), 286-292.
- de Vries, J. (2021). *Cubemaps*. LearnOpenGL. https://learnopengl.com/Advanced-OpenGL/Cubemaps
- de Vries, J. (2021). *Normal mapping*. LearnOpenGL. https://learnopengl.com/Advanced-Lighting/Normal-Mapping
- Humus. (n.d.). *Textures*. http://www.humus.name/index.php?page=Textures
- Khronos Group. (2024). *Cube map texture*. https://www.khronos.org/opengl/wiki/Cubemap_Texture
- Shreiner, D., Sellers, G., Kessenich, J., & Licea-Kane, B. (2013). *OpenGL programming guide* (8th ed.). Addison-Wesley.
