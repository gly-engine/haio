# codename-image-cdn-cli
image conversion and delivery tool, and also a "zcis" containerizer.

## Building on Windows

_If you don't have a basic MinGW toolchain on your Windows system, I recommend you try using [MYSYS2](https://www.msys2.org)._

```
pacman -S --needed mingw-w64-x86_64-{python,make,cmake,gcc}
```

```
cmake -Bbuild -H. -G "MinGW Makefiles"
```

```
mingw32-make -C build
```
