# OpenFunscripter
I swear this is a C++ project despite what the statistic on this repo says 😅

The project is based on OpenGL, SDL2, ImGui, & libmpv ( and I'm kind of proud of it ).

![OpenFunscripter Screenshot](https://github.com/gagax1234/OpenFunscripter/raw/master/OpenFunscripter.jpg)

### How to build ( for people who want to contribute or fork )
1. clone the repository
2. run `git submodule update --init --recursive`
3. Run cmake and compile ( on windows just use visual studio 2019 with cmake support )

Known linux dependencies to just compile are `build-essential libmpv-dev libglvnd-dev`.  
To compile something which runs on x11 and wayland other stuff is needed the snap includes support for both.

### Windows libmpv binaries used
Currently using: [mpv-dev-x86_64-20200816-git-7f67c52.7z (it's part of the repository)](https://sourceforge.net/projects/mpv-player-windows/files/libmpv/)


### Platforms
I'm providing windows binaries and a linux snap package.  
In theory OSX should work as well but I lack the hardware to set any of that up.
