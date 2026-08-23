# Reboot to slot1 cart

Bare minimum needed to have a slot1 cart reboot to itself, given that the homebrew
has access to the arm7 scfg registers.

There's no need for any of the dsi features to be enabled (bios/extra ram/extra
peripherals), not even the need to have arm9 code execution (the only caveat is
that slot1 needs to be given to the arm7), since all the logic of the program
runs on the arm7.

## Building

With blocksds installed, clone and run make

## License

Given that 99% of the code here is extracted from [blocksds' libnds](https://codeberg.org/blocksds/libnds) and [its crts](https://codeberg.org/blocksds/sdk/src/branch/master/sys/crts)
All files without explicit attribution are licensed under the Zlib license
