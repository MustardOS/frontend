# Pickles J2ME Test

A small MIDlet for checking FreeJ2ME under Pickles without needing any commercial content.

It sizes itself to whatever phone resolution the core gives it and shows:

- the resolution, frame rate and frame count, with colour bars and a moving square to judge the picture and its smoothness
- the uptime and a launch counter kept in phone storage (RMS), which proves game saves persist
- the last key code and game action received, the number of keys held, and the pointer position
- a tone played through the phone media API every time OK/Fire is pressed

Copy `PicklesTest.jar` to the device and launch it with the FreeJ2ME core.

## Rebuilding

`build.sh` compiles against `freej2me-lr.jar`, which supplies the phone APIs. It looks for the copy in the sibling `internal`
repository and also takes a path. Any Java 11 or newer runtime with the `jdk.compiler` and `jdk.jartool` modules works,
and `JAVA` picks a specific one.

```sh
./build.sh
./build.sh /path/to/freej2me-lr.jar
```

FreeJ2ME declares the `Canvas` key and pointer handlers `public` rather than the MIDP `protected`, so the overrides in the
source are `public` to match.
