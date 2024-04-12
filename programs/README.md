## muAudio - Hardware Level Audio Scaler

This little program will adjust ALSA at the hardware level
In muOS RetroArch is set to -12.0f audio level as baseline
Running will restore both speakers to 100% as they are separate to master volume
0% master volume or muting directly will switch the speaker off completely

* `VOLUME UP (PLUS)` - Raises the volume of the speaker
* `VOLUME DOWN (MINUS)` - Lowers the volume of the speaker
* `SELECT + DOWN` - Switches the device speaker power off

## muBright - Incremental Brightness Scaler

This little program will adjust the backlight of the RG35XX
Upon starting this program the level will be restored based on how it was upon shutdown
Turning the brightness to the lowest level will turn the screen off completely
Because of the increment used (16) there is a nice low level brightness for night time

* `SELECT + UP` - Increases the brightness until max level is hit (512)
* `SELECT + DOWN` - Decreases the brightness level

## muScreen - Framebuffer Screenshot to PNG

Exactly what you think it might do, takes a screenshot and outputs it to a PNG file
You have to choose what mountpoint to save the screenshots, either `mmc` to `sdcard`

* `VOLUP + POWER` - Take a screenshot, a slight rumble will occur for confirmation

## muShuffle - RetroArch ROM Shuffler

Running this will output a random ROM and associated RetroArch core to standard output
This is used in conjunction with RetroArch

## muSleep - A simplified sleep mechanism

This program will place the device in sleep/wake mode if you tap the power button
program detects idle input and will shut down as required

It will suspend all other programs including RetroArch

* `POWER` - Will place the device in sleep/wake mode

If no other arguments are specified muSleep will set the following:
* 5 Minutes (300 seconds) idle for the screen to go blank and mute the audio
* 10 Minutes (600 seconds) idle will sync the SD Card and shut it down safely

You can change those two parameters like so:
`mupower 400 800`

muPower does not care about your current game and will **not** save it

## muWatch - A process watcher for other muOS programs

Keeps a watch on several muOS programs to make sure they are still running
It has no other function other than starting programs if they quit abnormally
