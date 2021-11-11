# OpenFunscripter
I swear this is a C++ project despite what the statistic on this repo says 😅

The project is based on OpenGL, SDL2, ImGui, libmpv, & all these other great [libraries](https://github.com/OpenFunscripter/OpenFunscripter/tree/master/lib).

![OpenFunscripter Screenshot](https://github.com/OpenFunscripter/OpenFunscripter/blob/1b4f096be8c2f6c75ceed7787a300a86a13fb167/OpenFunscripter.jpg)

### How to build ( for people who want to contribute or fork )
1. clone the repository
2. `cd "OpenFunscripter"`
3. `git submodule update --init`
4. `cd "lib/EASTL"` 
5. `git submodule update --init`
6. Run cmake and compile (on windows just use visual studio 2019 with cmake support no need to generate a solution)

Known linux dependencies to just compile are `build-essential libmpv-dev libglvnd-dev`.  
To compile something which runs on x11 and wayland other stuff is needed the snap includes support for both.

### Windows libmpv binaries used
Currently using: [mpv-dev-x86_64-20200816-git-7f67c52.7z (it's part of the repository)](https://sourceforge.net/projects/mpv-player-windows/files/libmpv/)


### Platforms
I'm providing windows binaries and a linux AppImage.  
In theory OSX should work as well but I lack the hardware to set any of that up.
