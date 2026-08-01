# rcheevos dependency

Pickles vendors the official rcheevos 12.4.0 source at commit `2ad0b8672f68a48148620164510b963039e49eb1`.

The build includes `rc_client`, `rhash`, the libretro memory helper and their required runtime/API sources. Desktop integration and RetroAchievements Integration DLL sources are excluded. The upstream MIT licence is retained in `rcheevos/LICENSE`.

To update the dependency, review the upstream changelog and integration guide, replace the complete vendored tree, update the pinned version and commit here, then run the Pickles achievement, hashing, save-state and malformed-response tests.
