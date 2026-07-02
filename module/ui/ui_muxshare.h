#pragma once

#define CREATE_OPTION_ITEM(MODULE, NAME)                                                                               \
    do {                                                                                                               \
        ui_pnl_##NAME##_##MODULE = lv_obj_create(ui_pnl_content);                                                      \
        ui_lbl_##NAME##_##MODULE = lv_label_create(ui_pnl_##NAME##_##MODULE);                                          \
        lv_label_set_text(ui_lbl_##NAME##_##MODULE, "");                                                               \
        ui_ico_##NAME##_##MODULE = lv_img_create(ui_pnl_##NAME##_##MODULE);                                            \
        ui_dro_##NAME##_##MODULE = lv_dropdown_create(ui_pnl_##NAME##_##MODULE);                                       \
        lv_dropdown_clear_options(ui_dro_##NAME##_##MODULE);                                                           \
        lv_obj_set_style_text_opa(ui_dro_##NAME##_##MODULE, LV_OPA_TRANSP, MU_OBJ_INDI_DEFAULT);                       \
    } while (0)

#define CREATE_STATIC_ITEM(MODULE, NAME)                                                                               \
    do {                                                                                                               \
        ui_pnl_##NAME##_##MODULE = lv_obj_create(ui_pnl_content);                                                      \
        ui_lbl_##NAME##_##MODULE = lv_label_create(ui_pnl_##NAME##_##MODULE);                                          \
        lv_label_set_text(ui_lbl_##NAME##_##MODULE, "");                                                               \
        ui_ico_##NAME##_##MODULE = lv_img_create(ui_pnl_##NAME##_##MODULE);                                            \
    } while (0)

#define CREATE_VALUE_ITEM(MODULE, NAME)                                                                                \
    do {                                                                                                               \
        ui_pnl_##NAME##_##MODULE = lv_obj_create(ui_pnl_content);                                                      \
        ui_lbl_##NAME##_##MODULE = lv_label_create(ui_pnl_##NAME##_##MODULE);                                          \
        lv_label_set_text(ui_lbl_##NAME##_##MODULE, "");                                                               \
        ui_ico_##NAME##_##MODULE = lv_img_create(ui_pnl_##NAME##_##MODULE);                                            \
        ui_val_##NAME##_##MODULE = lv_label_create(ui_pnl_##NAME##_##MODULE);                                          \
        lv_label_set_text(ui_val_##NAME##_##MODULE, "");                                                               \
    } while (0)

#define CREATE_BAR_ITEM(MODULE, NAME)                                                                                  \
    do {                                                                                                               \
        ui_pnl_##NAME##_##MODULE = lv_obj_create(ui_pnl_content);                                                      \
        ui_pnl_##NAME##_bar_##MODULE = lv_obj_create(ui_pnl_content);                                                  \
        ui_lbl_##NAME##_##MODULE = lv_label_create(ui_pnl_##NAME##_##MODULE);                                          \
        ui_ico_##NAME##_##MODULE = lv_img_create(ui_pnl_##NAME##_##MODULE);                                            \
        ui_val_##NAME##_##MODULE = lv_label_create(ui_pnl_##NAME##_##MODULE);                                          \
        ui_bar_##NAME##_##MODULE = lv_bar_create(ui_pnl_##NAME##_bar_##MODULE);                                        \
        lv_label_set_text(ui_lbl_##NAME##_##MODULE, "");                                                               \
        lv_label_set_text(ui_val_##NAME##_##MODULE, "");                                                               \
        lv_obj_set_height(ui_pnl_##NAME##_bar_##MODULE, 10);                                                           \
        lv_obj_set_width(ui_pnl_##NAME##_bar_##MODULE, lv_pct(100));                                                   \
        lv_obj_set_width(ui_bar_##NAME##_##MODULE, lv_pct(100));                                                       \
        lv_bar_set_range(ui_bar_##NAME##_##MODULE, 0, device.mux.width);                                               \
        lv_obj_set_style_bg_color(                                                                                     \
            ui_bar_##NAME##_##MODULE, lv_color_hex(theme.verbose_boot.text), MU_OBJ_MAIN_DEFAULT                       \
        );                                                                                                             \
        lv_obj_set_style_bg_opa(ui_bar_##NAME##_##MODULE, 25, MU_OBJ_MAIN_DEFAULT);                                    \
        lv_obj_set_style_bg_color(                                                                                     \
            ui_bar_##NAME##_##MODULE, lv_color_hex(theme.verbose_boot.text), MU_OBJ_INDI_DEFAULT                       \
        );                                                                                                             \
        lv_obj_set_style_bg_opa(ui_bar_##NAME##_##MODULE, LV_OPA_COVER, MU_OBJ_INDI_DEFAULT);                          \
    } while (0)

#define APPCON_ELEMENTS                                                                                                \
    APPCON(governor, "governor")                                                                                       \
    APPCON(control, "control")

#define BACKUP_ELEMENTS                                                                                                \
    BACKUP(track, "track")                                                                                             \
    BACKUP(apps, "application")                                                                                        \
    BACKUP(music, "music")                                                                                             \
    BACKUP(content, "content")                                                                                         \
    BACKUP(collection, "collection")                                                                                   \
    BACKUP(override, "override")                                                                                       \
    BACKUP(package, "package")                                                                                         \
    BACKUP(name, "name")                                                                                               \
    BACKUP(history, "history")                                                                                         \
    BACKUP(catalogue, "catalogue")                                                                                     \
    BACKUP(network, "network")                                                                                         \
    BACKUP(cheats, "cheats")                                                                                           \
    BACKUP(config, "config")                                                                                           \
    BACKUP(overlays, "overlays")                                                                                       \
    BACKUP(shaders, "shaders")                                                                                         \
    BACKUP(save, "save")                                                                                               \
    BACKUP(screenshot, "screenshot")                                                                                   \
    BACKUP(syncthing, "syncthing")                                                                                     \
    BACKUP(bios, "bios")                                                                                               \
    BACKUP(theme, "theme")                                                                                             \
    BACKUP(init, "init")                                                                                               \
    BACKUP(target, "target")                                                                                           \
    BACKUP(merge, "merge")                                                                                             \
    BACKUP(start, "start")

#define BATINFO_ELEMENTS                                                                                               \
    BATINFO(capacity, "capacity")                                                                                      \
    BATINFO(voltage, "voltage")                                                                                        \
    BATINFO(status, "status")                                                                                          \
    BATINFO(health, "health")                                                                                          \
    BATINFO(design_cap, "designcap")                                                                                   \
    BATINFO(last_charged, "lastcharged")                                                                               \
    BATINFO(time_on_battery, "timeonbattery")                                                                          \
    BATINFO(battery_used, "batteryused")                                                                               \
    BATINFO(charger, "charger")

#define BTALL_ELEMENTS BTALL(auto_connect, "autoconnect")

#define BTDEV_INFO_ELEMENTS                                                                                            \
    BTDEV_INFO(friendly_name, "friendlyname")                                                                          \
    BTDEV_INFO(battery, "battery")                                                                                     \
    BTDEV_INFO(address, "address")

#define BTDEV_ACT_ELEMENTS                                                                                             \
    BTDEV_ACT(type, "type")                                                                                            \
    BTDEV_ACT(status, "status")                                                                                        \
    BTDEV_ACT(forget, "forget")

#define BTDEV_ELEMENTS                                                                                                 \
    BTDEV(friendly_name, "friendlyname")                                                                               \
    BTDEV(battery, "battery")                                                                                          \
    BTDEV(address, "address")                                                                                          \
    BTDEV(type, "type")                                                                                                \
    BTDEV(status, "status")                                                                                            \
    BTDEV(forget, "forget")

#define CHRONY_ELEMENTS                                                                                                \
    CHRONY(reference, "reference")                                                                                     \
    CHRONY(stratum, "stratum")                                                                                         \
    CHRONY(ref_time, "reftime")                                                                                        \
    CHRONY(system_time, "system")                                                                                      \
    CHRONY(last_offset, "last")                                                                                        \
    CHRONY(rms_offset, "rms")                                                                                          \
    CHRONY(frequency, "freq")                                                                                          \
    CHRONY(root_delay, "delay")                                                                                        \
    CHRONY(root_disp, "disp")                                                                                          \
    CHRONY(update_int, "update")                                                                                       \
    CHRONY(leap, "leap")

#define CONFIG_ELEMENTS                                                                                                \
    CONFIG(general, "general")                                                                                         \
    CONFIG(connect, "connect")                                                                                         \
    CONFIG(custom, "custom")                                                                                           \
    CONFIG(interface, "interface")                                                                                     \
    CONFIG(overlay, "overlay")                                                                                         \
    CONFIG(language, "language")                                                                                       \
    CONFIG(power, "power")                                                                                             \
    CONFIG(storage, "storage")                                                                                         \
    CONFIG(backup, "backup")

#define CONNECT_ELEMENTS                                                                                               \
    CONNECT(network, "network")                                                                                        \
    CONNECT(net_adv, "netadv")                                                                                         \
    CONNECT(proxy, "net_proxy")                                                                                        \
    CONNECT(services, "service")                                                                                       \
    CONNECT(bluetooth, "bluetooth")

#define CONTENT_ELEMENTS                                                                                               \
    CONTENT(launch_swap, "launch_swap")                                                                                \
    CONTENT(shuffle, "shuffle")                                                                                        \
    CONTENT(box_art_image, "boxart")                                                                                   \
    CONTENT(box_art_align, "align")                                                                                    \
    CONTENT(box_art_transition, "boxarttransition")                                                                    \
    CONTENT(box_art_scale, "boxartscale")                                                                              \
    CONTENT(box_art_padding, "boxartpadding")                                                                          \
    CONTENT(box_art_placeholder, "boxartplaceholder")                                                                  \
    CONTENT(video_preview, "videopreview")                                                                             \
    CONTENT(full_width, "width")                                                                                       \
    CONTENT(launch_splash, "splash")                                                                                   \
    CONTENT(grid_mode, "gridmodecontent")                                                                              \
    CONTENT(grid_mode_art, "boxarthide")

#define CUSTOM_ELEMENTS                                                                                                \
    CUSTOM(catalogue, "catalogue")                                                                                     \
    CUSTOM(config, "config")                                                                                           \
    CUSTOM(content_options, "content")                                                                                 \
    CUSTOM(font, "font")                                                                                               \
    CUSTOM(theme_opt, "themeopt")                                                                                      \
    CUSTOM(theme, "theme")                                                                                             \
    CUSTOM(theme_resolution, "resolution")                                                                             \
    CUSTOM(theme_scaling, "scaling")                                                                                   \
    CUSTOM(theme_alternate, "alternate")                                                                               \
    CUSTOM(video_wallpaper, "videowallpaper")                                                                          \
    CUSTOM(background_scale, "backgroundscale")                                                                        \
    CUSTOM(music, "music")                                                                                             \
    CUSTOM(music_volume, "musicvolume")                                                                                \
    CUSTOM(black_fade, "blackfade")                                                                                    \
    CUSTOM(sound, "sound")                                                                                             \
    CUSTOM(sound_volume, "soundvolume")                                                                                \
    CUSTOM(chime, "chime")

#define DANGER_ELEMENTS                                                                                                \
    DANGER(vm_swap, "vmswap")                                                                                          \
    DANGER(dirty_ratio, "dirty-ratio")                                                                                 \
    DANGER(dirty_back, "dirty-back")                                                                                   \
    DANGER(cache_pressure, "cache")                                                                                    \
    DANGER(no_merge, "merge")                                                                                          \
    DANGER(nr_requests, "requests")                                                                                    \
    DANGER(read_ahead, "readahead")                                                                                    \
    DANGER(page_cluster, "cluster")                                                                                    \
    DANGER(time_slice, "timeslice")                                                                                    \
    DANGER(io_stats, "iostats")                                                                                        \
    DANGER(idle_flush, "idleflush")                                                                                    \
    DANGER(child_first, "child")                                                                                       \
    DANGER(tune_scale, "tunescale")                                                                                    \
    DANGER(card_mode, "cardmode")                                                                                      \
    DANGER(state, "state")

#define DEVICE_ELEMENTS                                                                                                \
    DEVICE(has_bluetooth, "bluetooth")                                                                                 \
    DEVICE(has_rgb, "rgb")                                                                                             \
    DEVICE(has_debugfs, "debugfs")                                                                                     \
    DEVICE(has_hdmi, "hdmi")                                                                                           \
    DEVICE(has_lid, "lid")                                                                                             \
    DEVICE(has_network, "network")                                                                                     \
    DEVICE(has_portmaster, "portmaster")

#define DISTEMP_ELEMENTS                                                                                               \
    DISTEMP(schedule, "schedule")                                                                                      \
    DISTEMP(sunrise_temp, "sunrisetemp")                                                                               \
    DISTEMP(sunset_temp, "sunsettemp")                                                                                 \
    DISTEMP(sunrise_time, "sunrisetime")                                                                               \
    DISTEMP(sunset_time, "sunsettime")                                                                                 \
    DISTEMP(temp, "temp")

#define FONT_ELEMENTS                                                                                                  \
    FONT(type, "type")                                                                                                 \
    FONT(name, "name")                                                                                                 \
    FONT(list_size, "listsize")                                                                                        \
    FONT(header_size, "headersize")                                                                                    \
    FONT(footer_size, "footersize")                                                                                    \
    FONT(panel_size, "panelsize")

#define HDMI_ELEMENTS                                                                                                  \
    HDMI(resolution, "resolution")                                                                                     \
    HDMI(space, "space")                                                                                               \
    HDMI(depth, "depth")                                                                                               \
    HDMI(range, "range")                                                                                               \
    HDMI(scan, "scan")

#define INFO_ELEMENTS                                                                                                  \
    INFO(news, "news")                                                                                                 \
    INFO(activity, "activity")                                                                                         \
    INFO(screenshot, "screenshot")                                                                                     \
    INFO(space, "space")                                                                                               \
    INFO(tester, "tester")                                                                                             \
    INFO(sys_info, "sysinfo")                                                                                          \
    INFO(bat_info, "batinfo")                                                                                          \
    INFO(net_info, "netinfo")                                                                                          \
    INFO(chrony, "chrony")                                                                                             \
    INFO(credit, "credit")

#define INSTALL_ELEMENTS                                                                                               \
    INSTALL(rtc, "clock")                                                                                              \
    INSTALL(language, "language")                                                                                      \
    INSTALL(shutdown, "shutdown")                                                                                      \
    INSTALL(install, "install")

#define KIOSK_ELEMENTS                                                                                                 \
    KIOSK(enable, "enable")                                                                                            \
    KIOSK(message, "message")                                                                                          \
    KIOSK(archive, "archive")                                                                                          \
    KIOSK(task, "task")                                                                                                \
    KIOSK(custom, "custom")                                                                                            \
    KIOSK(language, "language")                                                                                        \
    KIOSK(network, "network")                                                                                          \
    KIOSK(storage, "storage")                                                                                          \
    KIOSK(backup, "backup")                                                                                            \
    KIOSK(net_adv, "netadv")                                                                                           \
    KIOSK(web_serv, "webserv")                                                                                         \
    KIOSK(core, "core")                                                                                                \
    KIOSK(governor, "governor")                                                                                        \
    KIOSK(control, "control")                                                                                          \
    KIOSK(option, "option")                                                                                            \
    KIOSK(retro_arch, "retroarch")                                                                                     \
    KIOSK(search, "search")                                                                                            \
    KIOSK(tag, "tag")                                                                                                  \
    KIOSK(col_filter, "colfilter")                                                                                     \
    KIOSK(shader, "shader")                                                                                            \
    KIOSK(rem_config, "remconfig")                                                                                     \
    KIOSK(catalogue, "catalogue")                                                                                      \
    KIOSK(ra_config, "raconfig")                                                                                       \
    KIOSK(theme, "theme")                                                                                              \
    KIOSK(theme_down, "theme_down")                                                                                    \
    KIOSK(clock, "clock")                                                                                              \
    KIOSK(timezone, "timezone")                                                                                        \
    KIOSK(apps, "apps")                                                                                                \
    KIOSK(config, "config")                                                                                            \
    KIOSK(explore, "explore")                                                                                          \
    KIOSK(collect_mod, "collectmod")                                                                                   \
    KIOSK(collect_add, "collectadd")                                                                                   \
    KIOSK(collect_new, "collectnew")                                                                                   \
    KIOSK(collect_rem, "collectrem")                                                                                   \
    KIOSK(collect_acc, "collectacc")                                                                                   \
    KIOSK(history_mod, "historymod")                                                                                   \
    KIOSK(history_rem, "historyrem")                                                                                   \
    KIOSK(info, "info")                                                                                                \
    KIOSK(rgb, "rgb")                                                                                                  \
    KIOSK(advanced, "advanced")                                                                                        \
    KIOSK(general, "general")                                                                                          \
    KIOSK(hdmi, "hdmi")                                                                                                \
    KIOSK(power, "power")                                                                                              \
    KIOSK(visual, "visual")                                                                                            \
    KIOSK(overlay, "overlay")

#define LAUNCH_ELEMENTS                                                                                                \
    LAUNCH(explore, "explore")                                                                                         \
    LAUNCH(collection, "collection")                                                                                   \
    LAUNCH(history, "history")                                                                                         \
    LAUNCH(apps, "apps")                                                                                               \
    LAUNCH(info, "info")                                                                                               \
    LAUNCH(config, "config")                                                                                           \
    LAUNCH(reboot, "reboot")                                                                                           \
    LAUNCH(shutdown, "shutdown")

#define NETADV_ELEMENTS                                                                                                \
    NETADV(monitor, "monitor")                                                                                         \
    NETADV(boot, "boot")                                                                                               \
    NETADV(wake, "wake")                                                                                               \
    NETADV(compat, "compat")                                                                                           \
    NETADV(async_load, "asyncload")                                                                                    \
    NETADV(con_retry, "conretry")                                                                                      \
    NETADV(wait, "wait")                                                                                               \
    NETADV(mod_retry, "modretry")

#define NETINFO_ELEMENTS                                                                                               \
    NETINFO(hostname, "hostname")                                                                                      \
    NETINFO(mac, "mac")                                                                                                \
    NETINFO(ip, "ip")                                                                                                  \
    NETINFO(ssid, "ssid")                                                                                              \
    NETINFO(gateway, "gateway")                                                                                        \
    NETINFO(dns, "dns")                                                                                                \
    NETINFO(signal, "signal")                                                                                          \
    NETINFO(channel, "channel")                                                                                        \
    NETINFO(ac_traffic, "actraffic")                                                                                   \
    NETINFO(tp_traffic, "tptraffic")

#define NETWORK_ELEMENTS                                                                                               \
    NETWORK(profile_name, "profile_name")                                                                              \
    NETWORK(identifier, "identifier")                                                                                  \
    NETWORK(password, "password")                                                                                      \
    NETWORK(type, "type")                                                                                              \
    NETWORK(priority, "priority")                                                                                      \
    NETWORK(address, "address")                                                                                        \
    NETWORK(subnet, "subnet")                                                                                          \
    NETWORK(gateway, "gateway")                                                                                        \
    NETWORK(dns, "dns")                                                                                                \
    NETWORK(connect, "connect")

#define OPTION_ELEMENTS                                                                                                \
    OPTION(core, "core")                                                                                               \
    OPTION(governor, "governor")                                                                                       \
    OPTION(control, "control")                                                                                         \
    OPTION(retro_arch, "retroarch")                                                                                    \
    OPTION(rem_config, "remconfig")                                                                                    \
    OPTION(col_filter, "colfilter")                                                                                    \
    OPTION(shader, "shader")                                                                                           \
    OPTION(tag, "tag")                                                                                                 \
    OPTION(storage, "storage")                                                                                         \
    OPTION(folder, "folder")                                                                                           \
    OPTION(name, "name")                                                                                               \
    OPTION(time, "time")                                                                                               \
    OPTION(launch, "launch")

#define OVERLAY_ELEMENTS                                                                                               \
    OVERLAY(gen_alpha, "gen_alpha")                                                                                    \
    OVERLAY(gen_anchor, "gen_anchor")                                                                                  \
    OVERLAY(gen_scale, "gen_scale")                                                                                    \
    OVERLAY(bat_alpha, "bat_alpha")                                                                                    \
    OVERLAY(bat_anchor, "bat_anchor")                                                                                  \
    OVERLAY(bat_scale, "bat_scale")                                                                                    \
    OVERLAY(vol_alpha, "vol_alpha")                                                                                    \
    OVERLAY(vol_anchor, "vol_anchor")                                                                                  \
    OVERLAY(vol_scale, "vol_scale")                                                                                    \
    OVERLAY(bri_alpha, "bri_alpha")                                                                                    \
    OVERLAY(bri_anchor, "bri_anchor")                                                                                  \
    OVERLAY(bri_scale, "bri_scale")

#define PASSCFG_ELEMENTS                                                                                               \
    PASSCFG(boot_code, "boot_lock")                                                                                    \
    PASSCFG(boot_msg, "boot_info")                                                                                     \
    PASSCFG(launch_code, "launch_lock")                                                                                \
    PASSCFG(launch_msg, "launch_info")                                                                                 \
    PASSCFG(setting_code, "setting_lock")                                                                              \
    PASSCFG(setting_msg, "setting_info")                                                                               \
    PASSCFG(safety_code, "safety")

#define POWER_ELEMENTS                                                                                                 \
    POWER(shutdown, "shutdown")                                                                                        \
    POWER(battery, "battery")                                                                                          \
    POWER(idle_sleep, "idle_sleep")                                                                                    \
    POWER(idle_display, "idle_display")                                                                                \
    POWER(idle_mute, "idle_mute")                                                                                      \
    POWER(gov_idle, "gov_idle")                                                                                        \
    POWER(gov_default, "gov_default")                                                                                  \
    POWER(saver_type, "saver_type")                                                                                    \
    POWER(saver_speed, "saver_speed")

#define PROXY_ELEMENTS                                                                                                 \
    PROXY(enabled, "enabled")                                                                                          \
    PROXY(type, "type")                                                                                                \
    PROXY(server, "server")                                                                                            \
    PROXY(no_proxy, "noproxy")                                                                                         \
    PROXY(test, "test")

#define RGB_ELEMENTS                                                                                                   \
    RGB(mode, "mode")                                                                                                  \
    RGB(bright, "bright")                                                                                              \
    RGB(breath_speed, "breath_speed")                                                                                  \
    RGB(colour_l, "colour_l")                                                                                          \
    RGB(colour_r, "colour_r")                                                                                          \
    RGB(colour_m, "colour_m")                                                                                          \
    RGB(colour_f1, "colour_f1")                                                                                        \
    RGB(colour_f2, "colour_f2")                                                                                        \
    RGB(combo, "combo")                                                                                                \
    RGB(backend, "backend")

#define RTC_ELEMENTS                                                                                                   \
    RTC(timezone, "timezone")                                                                                          \
    RTC(year, "year")                                                                                                  \
    RTC(month, "month")                                                                                                \
    RTC(day, "day")                                                                                                    \
    RTC(hour, "hour")                                                                                                  \
    RTC(minute, "minute")                                                                                              \
    RTC(notation, "notation")                                                                                          \
    RTC(custom, "custom")

#define SEARCH_ELEMENTS                                                                                                \
    SEARCH(lookup, "lookup")                                                                                           \
    SEARCH(search_local, "local")                                                                                      \
    SEARCH(search_global, "global")

#define SPACE_ELEMENTS                                                                                                 \
    SPACE(primary, "primary")                                                                                          \
    SPACE(secondary, "secondary")                                                                                      \
    SPACE(external, "external")                                                                                        \
    SPACE(system, "system")

#define STORAGE_ELEMENTS                                                                                               \
    STORAGE(apps, "apps")                                                                                              \
    STORAGE(bios, "bios")                                                                                              \
    STORAGE(catalogue, "catalogue")                                                                                    \
    STORAGE(collection, "collection")                                                                                  \
    STORAGE(history, "history")                                                                                        \
    STORAGE(init, "init")                                                                                              \
    STORAGE(music, "music")                                                                                            \
    STORAGE(name, "name")                                                                                              \
    STORAGE(network, "network")                                                                                        \
    STORAGE(package, "package")                                                                                        \
    STORAGE(save, "save")                                                                                              \
    STORAGE(screenshot, "screenshot")                                                                                  \
    STORAGE(syncthing, "syncthing")                                                                                    \
    STORAGE(theme, "theme")                                                                                            \
    STORAGE(track, "track")

#define SYSINFO_ELEMENTS                                                                                               \
    SYSINFO(version, "version")                                                                                        \
    SYSINFO(build, "build")                                                                                            \
    SYSINFO(device, "device")                                                                                          \
    SYSINFO(kernel, "kernel")                                                                                          \
    SYSINFO(arch, "arch")                                                                                              \
    SYSINFO(uptime, "uptime")                                                                                          \
    SYSINFO(boot_time, "boottime")                                                                                     \
    SYSINFO(load_avg, "loadavg")                                                                                       \
    SYSINFO(cpu, "cpu")                                                                                                \
    SYSINFO(speed, "speed")                                                                                            \
    SYSINFO(governor, "governor")                                                                                      \
    SYSINFO(memory, "memory")                                                                                          \
    SYSINFO(swap, "swap")                                                                                              \
    SYSINFO(temp, "temp")                                                                                              \
    SYSINFO(reload, "reload")

#define THEMEFILTER_ELEMENTS                                                                                           \
    THEMEFILTER(all_themes, "theme")                                                                                   \
    THEMEFILTER(grid, "grid")                                                                                          \
    THEMEFILTER(hdmi, "hdmi")                                                                                          \
    THEMEFILTER(language, "language")

#define THEMEOPT_ELEMENTS                                                                                              \
    THEMEOPT(header_height, "headerheight")                                                                            \
    THEMEOPT(footer_height, "footerheight")                                                                            \
    THEMEOPT(content_item_count, "count")                                                                              \
    THEMEOPT(glyph_list, "glyphlist")                                                                                  \
    THEMEOPT(glyph_footer, "glyphfooter")                                                                              \
    THEMEOPT(glyph_header, "glyphheader")                                                                              \
    THEMEOPT(glyph_grid, "glyphgrid")                                                                                  \
    THEMEOPT(label_width, "labelwidth")

#define TWEAKADV_ELEMENTS                                                                                              \
    TWEAKADV(accelerate, "accelerate")                                                                                 \
    TWEAKADV(repeat_delay, "repeat")                                                                                   \
    TWEAKADV(stick_nav, "sticknav")                                                                                    \
    TWEAKADV(volume, "volume")                                                                                         \
    TWEAKADV(brightness, "brightness")                                                                                 \
    TWEAKADV(thermal, "thermal")                                                                                       \
    TWEAKADV(led, "led")                                                                                               \
    TWEAKADV(random_theme, "randomtheme")                                                                              \
    TWEAKADV(retro_wait, "retrowait")                                                                                  \
    TWEAKADV(retro_free, "retrofree")                                                                                  \
    TWEAKADV(retro_cache, "retrocache")                                                                                \
    TWEAKADV(activity, "activity")                                                                                     \
    TWEAKADV(verbose, "verbose")                                                                                       \
    TWEAKADV(debug_log, "debuglog")                                                                                    \
    TWEAKADV(rumble, "rumble")                                                                                         \
    TWEAKADV(user_init, "userinit")                                                                                    \
    TWEAKADV(dpad_swap, "dpadswap")                                                                                    \
    TWEAKADV(overdrive, "overdrive")                                                                                   \
    TWEAKADV(lid_switch, "lidswitch")                                                                                  \
    TWEAKADV(disp_suspend, "dispsuspend")                                                                              \
    TWEAKADV(stage_overlay, "stageoverlay")                                                                            \
    TWEAKADV(swapfile, "swapfile")                                                                                     \
    TWEAKADV(zramfile, "zramfile")                                                                                     \
    TWEAKADV(second_part, "secondpart")                                                                                \
    TWEAKADV(usb_part, "usbpart")                                                                                      \
    TWEAKADV(inc_bright, "incbright")                                                                                  \
    TWEAKADV(inc_volume, "incvolume")                                                                                  \
    TWEAKADV(max_gpu, "maxgpu")                                                                                        \
    TWEAKADV(double_buffer, "doublebuffer")                                                                            \
    TWEAKADV(audio_ready, "audioready")                                                                                \
    TWEAKADV(audio_swap, "audioswap")                                                                                  \
    TWEAKADV(audio_suspend, "audiosuspend")                                                                            \
    TWEAKADV(bt_scan_timeout, "btscan")                                                                                \
    TWEAKADV(trust_modify, "trustmodify")                                                                              \
    TWEAKADV(trust_power, "trustpower")                                                                                \
    TWEAKADV(trust_remove, "trustremove")                                                                              \
    TWEAKADV(usb_function, "usbfunction")                                                                              \
    TWEAKADV(box_art_pad_div, "boxartpaddiv")

#define TWEAKGEN_ELEMENTS                                                                                              \
    TWEAKGEN(rtc, "clock")                                                                                             \
    TWEAKGEN(hdmi, "hdmi")                                                                                             \
    TWEAKGEN(rgb, "rgb")                                                                                               \
    TWEAKGEN(input_remap, "inputremap")                                                                                \
    TWEAKGEN(advanced, "advanced")                                                                                     \
    TWEAKGEN(pass_code, "lock")                                                                                        \
    TWEAKGEN(display_temp, "displaytemp")                                                                              \
    TWEAKGEN(brightness, "brightness")                                                                                 \
    TWEAKGEN(volume, "volume")                                                                                         \
    TWEAKGEN(audio_sink, "audiosink")                                                                                  \
    TWEAKGEN(hk_dpad, "hkdpad")                                                                                        \
    TWEAKGEN(hk_shot, "hkshot")                                                                                        \
    TWEAKGEN(startup, "startup")

#define VISUAL_ELEMENTS                                                                                                \
    VISUAL(sort, "sort")                                                                                               \
    VISUAL(battery, "battery")                                                                                         \
    VISUAL(clock, "clock")                                                                                             \
    VISUAL(network, "network")                                                                                         \
    VISUAL(bluetooth, "bluetooth")                                                                                     \
    VISUAL(header_title, "headertitle")                                                                                \
    VISUAL(dialogue_transition, "dialoguetransition")                                                                  \
    VISUAL(name, "name")                                                                                               \
    VISUAL(name_scroll, "namescroll")                                                                                  \
    VISUAL(label_scroll_speed, "labelscrollspeed")                                                                     \
    VISUAL(list_glyph, "listglyph")                                                                                    \
    VISUAL(selection_animation, "selectionanimation")                                                                  \
    VISUAL(selection_style, "selectionstyle")                                                                          \
    VISUAL(dash, "dash")                                                                                               \
    VISUAL(friendly_folder, "friendlyfolder")                                                                          \
    VISUAL(the_title_format, "thetitleformat")                                                                         \
    VISUAL(title_include_root_drive, "titleincluderootdrive")                                                          \
    VISUAL(folder_item_count, "folderitemcount")                                                                       \
    VISUAL(display_empty_folder, "folderempty")                                                                        \
    VISUAL(menu_counter_folder, "counterfolder")                                                                       \
    VISUAL(menu_counter_file, "counterfile")                                                                           \
    VISUAL(hidden, "hidden")                                                                                           \
    VISUAL(content_collect, "contentcollect")                                                                          \
    VISUAL(content_history, "contenthistory")                                                                          \
    VISUAL(mixed_content, "mixedcontent")                                                                              \
    VISUAL(forward_history, "forwardhistory")                                                                          \
    VISUAL(overlay_image, "overlayimage")                                                                              \
    VISUAL(overlay_transparency, "overlaytransparency")                                                                \
    VISUAL(render_shadows, "rendershadows")

#define WEBSERV_ELEMENTS                                                                                               \
    WEBSERV(sshd, "sshd")                                                                                              \
    WEBSERV(sftp_go, "sftpgo")                                                                                         \
    WEBSERV(ttyd, "ttyd")                                                                                              \
    WEBSERV(syncthing, "syncthing")                                                                                    \
    WEBSERV(tailscaled, "tailscaled")
