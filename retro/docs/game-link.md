# Game Link

Game Link exposes link-cable emulation supplied by a core. Gambatte currently provides the network Host and Client
modes, while compatible subsystem cores can provide Two Players on one device.

## Manual Link

Choose **Host** on one device and **Client** on the other. Enter the host device's IPv4 address on the client. Pickles
keeps manual Host, Client and Two Players modes independent from Direct Link.

## Direct Link

When MustardOS Direct Link reports a paired peer, Pickles can configure Game Link automatically. The device with the
lower Ethernet MAC address becomes the host and the other becomes the client. This is the same ordering used by the
Direct Link service when it assigns the deterministic `.1` and `.2` link-local addresses, so both devices reach the
same decision without another negotiation step.

A saved manual Host, Client or Two Players choice is respected. **Direct Link** is also available as an explicit Game
Link mode and displays **Waiting for peer** until the cable and peer are ready. Once selected, it monitors the Direct
Link state during play: losing the peer disables the core link connection, while a returning peer is configured again
without restarting Pickles.

## Pause Coordination

Two Pickles peers open a small UDP companion channel on the Game Link port plus one. It carries only a versioned
heartbeat and whether the local pause menu is open; the emulated link traffic remains owned by the core. Once the
companion channel is confirmed, opening either pause menu pauses both devices and the other device displays a message.

If the companion channel is unavailable or the other frontend does not support it, Pickles retains the legacy Game
Link behaviour and continues running the core while its own menu is open. This avoids deadlocking older peers.
