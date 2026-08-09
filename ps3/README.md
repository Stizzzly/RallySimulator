# PS3 build

This directory contains the PS3 RSXGL version of RallySimulator.

Requirements:

- PSL1GHT and ps3toolchain;
- RSXGL installed into `$(PS3DEV)/portlibs/ppu`;
- NVIDIA Cg Toolkit `cgc` available to `nv40c`.

Build from `ps3/rsxgl`:

```sh
make pkg PS3DEV=/usr/local/ps3dev PSL1GHT=/home/aleksandr/PSL1GHT \
  CGC=/path/to/cgc \
  LD_LIBRARY_PATH=/path/to/cg/lib
```

The package includes `USRDIR/car.obj`. Generated objects, shader headers,
ELF/SELF files and PKG files are intentionally ignored by Git.
