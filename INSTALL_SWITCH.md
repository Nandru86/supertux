## Installation

Copy `supertux2` folder to `sdmc:/switch/`. The structure should look like this:

```
SD
└── switch
    └── supertux2
        ├── supertux2.nro
        └── data

``` 

## Compiling for Nintendo switch:

Install DevKitPro with switch and arm64 support. also, install these extra libs:

`switch-libogg switch-libvorbis switch-openal-soft switch-freetype switch-harfbuzz switch-curl switch-libfribidi switch-glm switch-zlib switch-physfs switch-sdl2_image`

You also need to download, compile and install fmtlib onto devkitpro:

    download release 12.2.0 from https://github.com/fmtlib/fmt/releases/tag/12.2.0 and compile it:

    ```bash
    unzip fmt-12.2.0.zip
    cd fmt-12.2.0/
    cmake .. -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake -DPKG_CONFIG_EXECUTABLE=$DEVKITPRO/portlibs/switch/bin/aarch64-none-elf-pkg-config -DCMAKE_INSTALL_PREFIX=$DEVKITPRO/portlibs/switch -DFMT_TEST=false
    make
    sudo cmake --install .
    ```

Checkout the source code
```bash
git clone https://github.com/Nandru86/supertux.git
``` 

Be sure to be on the switch-port branch:
```bash
git branch --show-current
switch-port
```

Create a build directory

```bash
mkdir build
cd build
``` 

Modify line 40 of `external/simplesquirrel/CMakeLists.txt`, replacing `SHARED` with `STATIC`. Then, inside the `build` directory, use this cmake:
```bash
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$DEVKITPRO/cmake/Switch.cmake \
  -DPKG_CONFIG_EXECUTABLE=$DEVKITPRO/portlibs/switch/bin/aarch64-none-elf-pkg-config \
  -DHIDE_NONMOBILE_OPTIONS=ON \
  -DUSE_STATIC_SIMPLESQUIRREL=ON \
  -DSQ_DISABLE_INSTALLER=ON \
  -DENABLE_OPENGL=OFF
```

Compile and build the nro:
```bash
cmake --build .
```

You should now have a supertux2.nro in the build directory, along with the data folder on the root of the repository
