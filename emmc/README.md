# TrimUI eMMC Probe

A loader to bring the internal eMMC chipset found in various TrimUI devices to light.

The module itself makes no persistent hardware or eMMC changes. That's up to you!

Load manually:

```sh
insmod tui_emmc.ko
dmesg | tail -n 100
ls -l /dev/mmcblk*
```

Unload only while no eMMC partition is mounted:

```sh
rmmod tui_emmc
```
