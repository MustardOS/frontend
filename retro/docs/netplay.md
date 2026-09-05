# Network Play

Network Play lets several MustardOS devices play the same game together. One device hosts the game and up to three
other devices can join it. Each person needs their own device and their own copy of the same content.

## Before You Start

For the best result:

- connect every device to the same local network;
- use the same Pickles and core versions on every device;
- use exactly the same game file, active patches, core options and controller types; and
- keep each device close enough to its wireless access point for a stable signal.

A device on 2.4 GHz can play with one on 5 GHz when both bands belong to the same local network. Some routers isolate
wireless devices or keep the two bands separate. If no session appears, check that client or access-point isolation is
disabled.

## Hosting a Game

1. Start the content that everyone wants to play.
2. Open the pause menu and select **Network Play**.
3. Select **Host Network**.
4. Review the host settings:
   - **Host Name** is the friendly name shown to other devices. MustardOS creates a random name by default.
   - **Play Mode** chooses how controllers are shared.
   - **Client Slots** chooses how many clients must join, from one to three.
   - **Host Port** changes the network port when the default is unavailable.
5. Select **Host Session** and press A.
6. Leave the session open while the other players join.

The host displays how many clients have confirmed and how many are still required. Play begins automatically after all
selected client slots have joined, confirmed their pairing codes and passed the compatibility checks.

## Joining a Game

1. Start the same content with the same core.
2. Open **Network Play**, then select **Join Network**.
3. Select **Find LAN Sessions**.
4. Highlight **Join Found Session**. Use left and right when more than one host was found, then press A to join.
5. Check that the displayed pairing code matches the code on the host.
6. Highlight **Confirm Pairing** and press A.

If discovery is unavailable, select **Join by Address** and enter the host's name or IP address. A custom port can be
added after the address. When an IPv6 address also includes a port, place the address in square brackets, such as
`[2001:db8::10]:55435`.

## Play Modes

### Separate Players

The host controls Player 1. Joined clients are assigned to Players 2, 3 and 4 in connection order. Use this for games
with normal local multiplayer support.

### Play Together

Every connected device controls Player 1. This is useful when helping another player, sharing a single-player game or
taking turns without passing one device around. Simultaneous opposing directions cancel each other.

## What to Expect During Play

- Pickles keeps every device on the same emulated timeline. A weak or busy connection may cause a brief pause rather
  than allowing the games to drift apart.
- If anyone opens the pause menu, all devices pause on a shared frame. Devices without the menu open show a dimmed
  message as soon as the request arrives, and keep it visible until every open pause menu has closed.
- The performance governor remains stable for the whole session and returns to its previous setting afterwards.
- Run-ahead, fast-forward, slow motion, local state loading, timeline states, cheats and achievement processing are
  suspended while Network Play is active.
- Select **Disconnect** from the main Network Play menu to end the session cleanly.

## If a Session Will Not Connect

Check the status shown in the Network Play menu. Common causes include:

- different Pickles or core versions;
- different game files, patches, core options or controller types;
- a router that prevents wireless devices from communicating with each other;
- a firewall blocking discovery or the selected host port; or
- wireless packet loss that exceeds the recovery window.

If LAN discovery fails but the devices can otherwise reach each other, try **Join by Address**. If a running session
stops, the displayed message distinguishes a peer closure, connection timeout, invalid protocol data, state mismatch or
an input backlog that exceeded its recovery limit.

## Brief Technical Notes

- LAN discovery uses UDP port `55436`. Game sessions use TCP port `55435` by default, with `57193`, `59387`, `61231`
  and `63863` available from the menu.
- Session traffic uses TLS 1.3 and ephemeral mutual certificates. The six-digit pairing code is derived from both peers'
  session certificates; game content and account credentials are not transferred.
- The bounded `PKNP/4` protocol carries input, timing, pause and synchronisation messages with sequence and frame
  numbers.
- Normal cores use exact-input lockstep with an adaptive delay of two to eight frames. A 256-frame transport history
  absorbs temporary network jitter without adding normal input delay.
- The host compares serialised state digests every 300 frames. It attempts one authoritative resynchronisation before
  ending a session that continues to diverge.
- Cores that expose the libretro netpacket interface manage their own game networking through the separate core-managed
  path.
