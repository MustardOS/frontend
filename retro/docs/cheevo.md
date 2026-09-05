# Achievements

Pickles integrates with RetroAchievements so supported games can award achievements, points and leaderboard results
while you play on MustardOS. You need a RetroAchievements account and an internet connection for initial login and game
identification.

## Logging In

1. Start any content and open the pause menu.
2. Select **Settings**, then **RetroAchievements**.
3. Open **Account**.
4. Enter your RetroAchievements **Username** and **Password** separately.
5. Select **Login** and press A.

The login is global: once saved, the same account is used for every supported core and item of content. The entered
password is obscured and cleared from memory after the login request. MustardOS saves the returned account token instead
of saving the password, and restricts the account file to the current system user.

Use **Logout** or press X for **Reset** in the account screen when you want to log out and clear the saved account. A
confirmation dialogue prevents accidental removal.

## Starting a Game

When configured, Pickles identifies supported content as it starts. You may briefly see a connecting message while this
happens. **RetroAchievements Active** confirms that the game was recognised and achievement processing is ready.

The **Achievements** item appears in the main pause menu only after an account has been configured. A game may still
show no achievements when:

- RetroAchievements does not recognise that exact game revision;
- the selected core does not expose usable game memory;
- the network is unavailable and suitable data has not been cached; or
- the game has no published achievement set.

## Softcore and Hardcore

Select **Mode** in the RetroAchievements settings to switch between:

- **Softcore**, which permits the normal Pickles convenience features; and
- **Hardcore**, which applies RetroAchievements restrictions and resets the content when enabled.

Hardcore restricts features that could alter or restore gameplay, including state loading, run-ahead, cheats, RELISH
macros, turbo, fast-forward and slow motion. Pausing may also be delayed until RetroAchievements reports that it is safe.
Hardcore cannot be enabled during Network Play or while gameplay patches are active.

## Notifications

Use left or right on **Notifications** to choose:

- **Disabled** -- no normal achievement notifications;
- **Basic** -- shows an achievement only when it is awarded; or
- **Detailed** -- also shows measured progress, active challenges and leaderboard activity.

An awarded achievement uses a short trophy notification containing its name and point value. Pickles also captures a
clean gameplay screenshot for that achievement when possible.

## Browsing Achievements

Open the pause menu and select **Achievements**.

- Press A on an achievement to view its description.
- Press left or right to switch between **Preview** and **List** for achievements. A saved unlock screenshot appears in
  Preview when one is available.
- Press X for **Mode** to switch between achievements and leaderboards.
- Press Y for **Sort** while viewing achievements.
- Press B to return to the main pause menu.

The selected achievement or leaderboard mode and sort order are remembered globally. Available achievement sorts are
display order first, display order last, alphanumeric ascending, alphanumeric descending, points highest, points lowest,
percentage common, percentage rarest, unlocked first and easy points.

In leaderboard mode, press A on a leaderboard to request its details. Pickles displays up to the top ten returned
entries, including the current user when supplied by RetroAchievements.

## Cached Data and Refreshing

Pickles caches suitable achievement responses so previously loaded information can be reused and startup does not need
to download everything again. If data appears outdated, open **Settings** > **RetroAchievements** and select
**Refresh Data**. Refreshing tries the service first and reloads the current game's achievement information. If the
connection drops during the refresh, Pickles falls back to the last usable cached copy when one is available.

If the service becomes unavailable during play, Pickles enters an offline state and lets the RetroAchievements client
retry its pending work. Interrupted token sign-in and game identification are retried in the background with increasing
delays. A reconnect notification appears when service returns. Cached descriptions and saved previews may remain
available, but new awards and leaderboard submissions still depend on the server accepting them.

## Brief Technical Notes

- Pickles uses the official `rcheevos` client and `rhash` content-identification support rather than implementing
  achievement rules itself.
- HTTP work runs away from the emulation frame loop through a bounded asynchronous queue. Responses are size-limited and
  TLS certificates are verified.
- Account storage contains the username and returned token, never the entered password. The directory uses owner-only
  permissions, files are written atomically and unsafe links or ownership are rejected.
- Unlocks and leaderboard results are written to an owner-only durable queue before transmission. Held records use
  increasing retry delays and are removed only after an explicit successful API response.
- The response cache is limited to 32 MiB and entries expire after 30 days. **Refresh Data** bypasses the normal cached
  game-data lookup.
- Unlock previews are stored per game and achievement. Missing or failed captures safely fall back to the normal trophy
  presentation.
- Achievement progress is included with compatible Pickles save states in Softcore mode. Loading states is unavailable
  in Hardcore mode.
- Achievement processing enters spectator mode and is suspended for the duration of a Network Play session.
