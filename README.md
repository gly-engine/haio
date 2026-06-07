# codename-image-cdn-cli
image conversion and delivery tool, and also a "zcis" containerizer.

## Building on Windows

_If you don't have a basic MinGW toolchain on your Windows system, I recommend you try using [MYSYS2](https://www.msys2.org)._

```
pacman -S python mingw-w64-x86_64-make mingw-w64-x86_64-cmake mingw-w64-x86_64-gcc
```

```
cmake -Bbuild -H. -G "MinGW Makefiles"
```

```
mingw32-make -C build
```
