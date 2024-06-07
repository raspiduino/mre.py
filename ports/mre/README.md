# The MRE port

This MicroPython port is for [MediaTek MRE](https://lpcwiki.miraheze.org/wiki/MAUI_Runtime_Environment)-based feature phones, including, but not limited to Nokia S30+ series (Nokia 220, Nokia 225,...)

## Current status
- [x] Built-in terminal
- [x] Compiler running
- [x] Single-line REPL
- [ ] Multi-line REPL
- [ ] `.py` file execution
- [ ] Standard MRE-specific modules
- [ ] Nokia S30+ specific modules (interact with hardware)

## Building

First, you need to clone this repo, by:
```bash
git clone https://github.com/raspiduino/mrepl
cd mrepl
```

You may also prefer a light clone with `--depth=1`.

Next, init the `mre-makefile` submodule (containing SDK, required for building MRE app) by

```bash
git submodule init
git submodule update ports/mre/mre-makefile
```

To compile the source, you need a toolchain. A recommended toolchain would be [`gcc-arm-none-eabi` version 10.3](https://developer.arm.com/downloads/-/gnu-rm) (You can choose other toolchain/version if you would like). Download the suitable toolchain for your host OS and architecture, and add it bin path to `PATH`. For example, with Linux x86_64, it would be:

```bash
wget https://developer.arm.com/-/media/Files/downloads/gnu-rm/10.3-2021.10/gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
tar -xvf gcc-arm-none-eabi-10.3-2021.10-x86_64-linux.tar.bz2
export PATH=$PATH:$PWD/gcc-arm-none-eabi-10.3-2021.10/bin/
```

Finally, you can build MicroPython for MRE:

```bash
cd ports/mre
make
```

If the build success, you will get a VXP at `build/mrepl.vxp`. Then, you can copy it to your MRE device, sign it, and run it like a normal application.

## Credit
- Original [MicroPython](https://github.com/micropython/micropython) by MicroPython contributors
- Me ([gvl610](https://github.com/raspiduino)) for porting MicroPython to MRE
- [Ximik_Boda](https://github.com/XimikBoda) for creating [TelnetVXP](https://github.com/XimikBoda/TelnetVXP) from which terminal is adapted and for helping me a lot
- [gtrxAC](https://github.com/gtrxAC) for creating [mre-makefile](https://github.com/gtrxAC/mre-makefile) and for helping me a lot
