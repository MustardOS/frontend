# muX
muOS Frontend

## Configuration
- [buildroot/Batocera Lite SDK Toolchain](https://github.com/rg35xx-cfw/rg35xx-cfw.github.io/releases)
    - Unpack the tarfile, and then run the `./relocate.sh` in the root directory to update the buildroot SDK accordingly.

The `setvars.sh` script included in this file prepares the environment variables for the current session to build for the target device.
    - This shell script assumes your toolchain directory lives at `~/x-tools`.

To build, a target device must be exported to environment variable `DEVICE`. Device options are located in `common/help.h`, and include:

- RG28XX
- RG32XXH
- RG35XXOG
- RG35XXPLUS
- RG35XXSP
- RG35XX2024

## Build
To build, first run `. ./setvars.sh` to set the correct environment variables for the current session, and then `./buildall.sh`, which will make all of the individual subfolders.
