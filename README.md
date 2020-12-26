# Pulseaudio Persistent default sink Daemon
**papersistd** is a demo project of system daemon that wait for audio sink with specific name and
make it PulseAudio default sink.

# How-to
## Configure
```shell
$ conan install . -if build # Optional. Skip if system dependencies are used.
$ export PKG_CONFIG_PATH="$PWD/build" # Optional. Skip if system dependencies are used.
$ cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$HOME/.local/bin"
```
## Build
```shell
$ cmake --build build
```
## Install
```shell
$ cmake --build --target install
```
Also it is required to set environment variables:
```shell
$ echo "PULSE_SERVER=unix:/run/user/1000/pulse/native" >> ~/.config/environment.d/papersistd.conf # Optional.
$ echo "PULSE_DEFAULT_SINK=alsa_output.some-name.analog-stereo" >> ~/.config/environment.d/papersistd.conf
```
and start the daemon:
```shell
$ systemctl --user start papersistd
```

# Dependencies
## Build dependencies
* [Conan](https://conan.io) package manager **(Optional)**
* [CMake](https://cmake.org) build tool
* [pkg-config](https://www.freedesktop.org/wiki/Software/pkg-config/)
* C++ toolchain with C++17 support *(other version is not tested)*
* libpulse - The asynchronous PulseAudio API library and headers.
## Runtime dependencies
* [PulseAudio](https://freedesktop.org/software/pulseaudio/doxygen/)
* [systemd](https://www.freedesktop.org/wiki/Software/systemd/)
