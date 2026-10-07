#!/usr/bin/env python3
import argparse
import json
import os
import re
import sys

PROFILE_GROUPS = [
    ("settings/general", "general", "General Settings"),
    ("settings/advanced", "advanced", "Advanced Settings"),
    ("settings/font", "font", "Font"),
    ("settings/power", "power", "Power"),
    ("settings/hotkey", "hotkey", "Hotkeys"),
    ("settings/colour", "colour", "Display Colour"),
    ("settings/hdmi", "hdmi", "HDMI"),
    ("settings/overlay", "overlay", "Overlays"),
    ("settings/theme", "themeoverride", "Theme Overrides"),
    ("visual", "visual", "Customisation"),
    ("web", "web", "Web Services"),
]

OEM_ONLY_PREFIXES = ("network/", "settings/network/")
OEM_ONLY_EXCLUDED = {"network/interface"}

WIFI_LABELS = {
    "network/ssid": "Network name",
    "network/pass": "Password, leave it empty for an open network",
    "network/type": "dhcp to be given an address automatically, or static to set one yourself",
    "network/address": "Static address, such as 192.168.0.123",
    "network/subnet": "Static subnet prefix length, such as 24",
    "network/gateway": "Static gateway address",
    "network/dns": "Static DNS server address",
    "settings/network/proxy_noproxy": "Addresses that skip the proxy, separated by commas",
}

WIFI_ORDER = {key: index for index, key in enumerate(WIFI_LABELS) if key.startswith("network/")}

REFUSED_REASONS = [
    ("web/ttyd_user", "Login details cannot be set by a profile"),
    ("web/ttyd_pass", "Login details cannot be set by a profile"),
    ("network/interface", "The network interface depends on the device and cannot be set by a profile"),
    ("settings/advanced/passcode", "The passcode cannot be set by a profile"),
    ("settings/advanced/user_init", "User start up scripts cannot be turned on by a profile"),
    ("kiosk/", "Kiosk restrictions cannot be set by a profile"),
    ("network/", "Network settings cannot be set by a profile"),
    ("settings/network/", "Network settings cannot be set by a profile"),
    ("web/", "Web service settings cannot be set by a profile"),
    ("terminal/", "Terminal settings cannot be set by a profile"),
    ("backup/", "Backup choices cannot be set by a profile"),
    ("clock/", "The clock cannot be set by a profile"),
    ("danger/", "Danger settings cannot be set by a profile"),
    ("settings/general/audiosink", "The audio output is chosen on each device and cannot be set by a profile"),
    ("settings/general/brightness", "Brightness is adjusted on the device and cannot be set by a profile"),
    ("settings/general/volume", "Volume is adjusted on the device and cannot be set by a profile"),
    ("settings/rgb/", "RGB lighting is set by the active theme and cannot be set by a profile"),
    ("theme/", "Theme choices cannot be set by a profile"),
    ("sort/", "Content sorting cannot be set by a profile"),
    ("", "This setting is managed by the system and cannot be set by a profile"),
]

COMBO_SOURCES = {
    "settings/hotkey/dpad_toggle": "share/hotkey/rg.ini",
    "settings/hotkey/screenshot": "share/hotkey/rg.ini",
}

LABEL_WORDS = {
    "bat": "Battery",
    "bri": "Brightness",
    "gen": "General",
    "vol": "Volume",
    "bt": "Bluetooth",
    "cpu": "CPU",
    "gpu": "GPU",
    "max": "Maximum",
    "rgb": "RGB",
}

DISABLED_ENABLED = [("0", "lang.generic.disabled"), ("1", "lang.generic.enabled")]
GLYPH_SIZES = (8, 12, 16, 20, 24, 28, 32, 36, 40, 48, 56, 64, 80, 96, 128)

EXPLICIT_OPTIONS = {
    "settings/power/shutdown": ("muxpower", "sleep_timer", "shutdown_values", []),
    "settings/power/idle_display": ("muxpower", "idle_timer", "idle_values", []),
    "settings/power/idle_sleep": ("muxpower", "idle_timer", "idle_values", []),
    "settings/power/saver_type": ("muxpower", "saver_type", "saver_type_values", []),
    "settings/power/saver_speed": ("muxpower", "saver_speed", "saver_speed_values", [("600", "lang.generic.ludicrous")]),
    "settings/hdmi/space": ("muxhdmi", "hdmi_space", None, []),
    "settings/hdmi/resolution": ("muxhdmi", "hdmi_resolution", None, []),
}
for _part in ("gen", "bat", "vol", "bri"):
    EXPLICIT_OPTIONS[f"settings/overlay/{_part}_anchor"] = ("muxoverlay", "anchor_options", None, [])
    EXPLICIT_OPTIONS[f"settings/overlay/{_part}_scale"] = ("muxoverlay", "scale_options", None, [])

FIXED_OPTIONS = {
    "visual/highcontrast": DISABLED_ENABLED,
    "visual/boldfocus": DISABLED_ENABLED,
    "visual/boxarthide": DISABLED_ENABLED,
    "settings/advanced/overdrive": DISABLED_ENABLED,
    "visual/reducemotion": [("0", "lang.generic.disabled"), ("1", "lang.muxprofile.reduced"), ("2", "lang.generic.maximum")],
    "settings/advanced/font": [("0", "lang.muxfont.type_options.language"), ("1", "lang.muxfont.type_options.theme"),
                               ("2", "lang.muxfont.type_options.internal"), ("3", "lang.muxfont.type_options.custom")],
    "settings/general/theme_resolution": [("0", "lang.muxcustom.screen")] + [
        (str(index), size) for index, size in enumerate(
            ("640x480", "720x480", "720x576", "720x720", "1024x768", "1280x720", "1920x1080"), 1)],
    "settings/advanced/accelerate": [("32767", "lang.generic.disabled")] + [(str(v), str(v)) for v in range(16, 257, 16)],
    "settings/advanced/repeat_delay": [("32767", "lang.generic.disabled")] + [(str(v), str(v)) for v in range(16, 513, 16)],
    "settings/advanced/swapfile": [("0", "lang.generic.disabled")] + [(str(v), f"{v} MB") for v in range(128, 1025, 128)],
    "settings/advanced/zramfile": [("0", "lang.generic.disabled")] + [(str(v), f"{v} MB") for v in range(128, 1025, 128)],
    "settings/colour/sunrise_time": [(str(i), f"{i * 15 // 60:02d}:{i * 15 % 60:02d}") for i in range(96)],
    "settings/colour/sunset_time": [(str(i), f"{i * 15 // 60:02d}:{i * 15 % 60:02d}") for i in range(96)],
    "settings/theme/header_height": [("-1", "lang.muxthemeopt.size_default")] + [(str(v), str(v)) for v in range(65)],
    "settings/theme/footer_height": [("-1", "lang.muxthemeopt.size_default")] + [(str(v), str(v)) for v in range(65)],
}
for _glyph in ("footer", "grid", "header", "list"):
    FIXED_OPTIONS[f"settings/theme/glyph_size_{_glyph}"] = [
        ("-2", "lang.muxthemeopt.size_default"), ("0", "lang.muxthemeopt.glyph_auto"),
        ("-1", "lang.muxthemeopt.glyph_native"),
    ] + [(str(v), str(v)) for v in GLYPH_SIZES]

HELP_KEYS = {
    "web/mdns": "muxwebserv.help.mdns",
    "web/mdns_name": "muxwebserv.help.local_name",
    "web/landing": "muxwebserv.help.landing",
    "web/landing_port": "muxwebserv.help.web_port",
    "web/landing_auth": "muxwebserv.help.authentication",
    "web/remote_view": "muxwebserv.help.remote_view",
    "web/remote_privacy": "muxwebserv.help.remote_privacy",
    "web/sshd": "muxwebserv.help.sshd",
    "web/sshd_port": "muxwebserv.help.port",
    "web/sftpgo": "muxwebserv.help.sftp_go",
    "web/sftpgo_port": "muxwebserv.help.web_port",
    "web/sftpgo_sftp_port": "muxwebserv.help.sftp_port",
    "web/ttyd": "muxwebserv.help.service",
    "web/ttyd_port": "muxwebserv.help.port",
    "web/syncthing": "muxwebserv.help.syncthing",
    "web/syncthing_port": "muxwebserv.help.web_port",
    "web/tailscaled": "muxwebserv.help.tailscaled",
    "settings/advanced/bt_scan_timeout": "muxtweakadv.help.bt_scan_timeout",
    "settings/advanced/font": "muxfont.help.type",
    "settings/advanced/overdrive": "muxtweakadv.help.overdrive",
    "settings/font/directory": "muxfont.help.font_directory",
    "settings/font/name": "muxfont.help.font_name",
    "settings/font/width": "muxfont.help.width",
    "settings/font/italic": "muxfont.help.italic",
    "settings/font/list_size": "muxfont.help.list_size",
    "settings/font/header_size": "muxfont.help.header_size",
    "settings/font/footer_size": "muxfont.help.footer_size",
    "settings/font/panel_size": "muxfont.help.panel_size",
    "settings/font/scale": "muxfont.help.text_scale",
    "settings/general/language": "muxtweakgen.help.language",
    "settings/general/soundfont": "muxtweakgen.help.soundfont",
    "settings/general/theme_resolution": "muxcustom.help.theme_resolution",
    "settings/power/idle_display": "muxpower.help.idle_display",
    "settings/power/idle_sleep": "muxpower.help.idle_sleep",
    "settings/power/low_battery": "muxpower.help.battery",
    "settings/power/saver_speed": "muxpower.help.saver_speed",
    "settings/power/saver_type": "muxpower.help.saver_type",
    "settings/power/shutdown": "muxpower.help.shutdown",
    "settings/theme/content_item_count": "muxthemeopt.help.content_item_count",
    "settings/theme/glyph_size_footer": "muxthemeopt.help.glyph_footer",
    "settings/theme/glyph_size_grid": "muxthemeopt.help.glyph_grid",
    "settings/theme/glyph_size_header": "muxthemeopt.help.glyph_header",
    "settings/theme/glyph_size_list": "muxthemeopt.help.glyph_list",
    "settings/theme/label_width": "muxthemeopt.help.label_width",
    "visual/boxart": "muxvisual.help.box_art",
    "visual/boxarthide": "muxvisual.help.box_art_hide",
    "visual/contentwidth": "muxvisual.help.content_width",
    "visual/gridmodecontent": "muxvisual.help.grid_mode_content",
    "visual/launchsplash": "muxvisual.help.launchsplash",
    "visual/overlayimage": "muxvisual.help.overlay_image",
    "visual/overlaytransparency": "muxvisual.help.overlay_transparency",
    "visual/pickles_startup_messages": "muxvisual.help.pickles_startup_messages",
    "settings/font/face": "muxfont.help.face",
    "visual/boldfocus": "muxprofile.help.bold_focus",
    "visual/highcontrast": "muxprofile.help.high_contrast",
    "visual/reducemotion": "muxprofile.help.reduce_motion",
}
for _part in ("gen", "bat", "vol", "bri"):
    for _field in ("alpha", "anchor", "scale"):
        HELP_KEYS[f"settings/overlay/{_part}_{_field}"] = f"muxoverlay.help.{_part}_{_field}"

PERCENT = [(str(v), f"{v}%") for v in range(101)]


def percent_of(maximum):
    return [(str((v * maximum + 50) // 100), f"{v}%") for v in range(101)]


FIXED_OPTIONS.update({
    "settings/general/startup": [
        ("launcher", "lang.muxtweakgen.startup.menu"), ("explore", "lang.muxtweakgen.startup.explore"),
        ("collection", "lang.muxtweakgen.startup.collection"), ("history", "lang.muxtweakgen.startup.history"),
        ("last", "lang.muxtweakgen.startup.last"), ("resume", "lang.muxtweakgen.startup.resume"),
    ],
    "settings/advanced/bt_scan_timeout": [(str(v), f"{v} {{lang.muxtweakadv.seconds}}") for v in range(5, 31, 5)],
    "settings/advanced/boxartpaddiv": [(str(i), str(v)) for i, v in enumerate((50, 100, 200, 400, 600, 800))],
    "settings/advanced/incbright": [(str(v), str(v)) for v in range(1, 33)],
    "settings/advanced/incvolume": [(str(v), str(v)) for v in range(1, 33)],
    "settings/general/bgmvol": PERCENT,
    "settings/general/soundvol": PERCENT,
    "settings/power/low_battery": PERCENT,
    "visual/overlaytransparency": percent_of(255),
    "visual/boxartscale": [("0", "lang.generic.disabled")] + PERCENT[1:],
    "visual/boxartpadding": [("0", "lang.generic.disabled")] + PERCENT[1:],
    "settings/theme/content_item_count": [("0", "lang.muxthemeopt.size_default")] + [(str(v), str(v)) for v in range(3, 65)],
    "settings/theme/label_width": [("0", "lang.muxthemeopt.size_default")] + [(str(v), f"{v}%") for v in range(10, 101)],
})
for _part in ("gen", "bat", "vol", "bri"):
    FIXED_OPTIONS[f"settings/overlay/{_part}_alpha"] = percent_of(255)

FIXED_OPTIONS["settings/font/scale"] = [(str(v), f"{v}%") for v in (75, 85, 100, 115, 130, 145, 160, 175, 200)]

FIXED_OPTIONS["web/remote_view"] = [("0", "lang.generic.disabled"), ("1", "30s"), ("2", "1m"), ("3", "3m"),
                                    ("4", "5m"), ("5", "10m")]

OPEN_OPTIONS = {"settings/general/soundfont", "visual/overlaytransparency"}
OPEN_OPTIONS.update(f"settings/overlay/{part}_alpha" for part in ("gen", "bat", "vol", "bri"))

FIXED_RANGES = {
    "settings/power/low_battery": (0, 100),
    "settings/general/bgmvol": (0, 100),
    "settings/general/soundvol": (0, 100),
    "visual/overlaytransparency": (0, 255),
}
for _part in ("gen", "bat", "vol", "bri"):
    FIXED_RANGES[f"settings/overlay/{_part}_alpha"] = (0, 255)

LABEL_KEYS = {
    "settings/advanced/font": "lang.muxfont.type",
    "web/mdns": "lang.muxwebserv.mdns",
    "web/mdns_name": "lang.muxwebserv.local_name",
    "web/landing": "lang.muxwebserv.landing",
    "web/landing_port": "{lang.muxwebserv.landing} {lang.muxwebserv.port}",
    "web/landing_auth": "{lang.muxwebserv.landing} {lang.muxwebserv.authentication}",
    "web/remote_view": "lang.muxwebserv.remote_view",
    "web/remote_privacy": "lang.muxwebserv.remote_privacy",
    "web/sshd": "lang.muxwebserv.sshd",
    "web/sshd_port": "{lang.muxwebserv.sshd} {lang.muxwebserv.port}",
    "web/sftpgo": "lang.muxwebserv.sftpgo",
    "web/sftpgo_port": "{lang.muxwebserv.sftpgo} {lang.muxwebserv.web_port}",
    "web/sftpgo_sftp_port": "{lang.muxwebserv.sftpgo} {lang.muxwebserv.sftp_port}",
    "web/ttyd": "lang.muxwebserv.ttyd",
    "web/ttyd_port": "{lang.muxwebserv.ttyd} {lang.muxwebserv.port}",
    "web/syncthing": "lang.muxwebserv.syncthing",
    "web/syncthing_port": "{lang.muxwebserv.syncthing} {lang.muxwebserv.web_port}",
    "web/tailscaled": "lang.muxwebserv.tailscaled",
}

LABEL_LEAVES = {
    "audiosink": "Audio Sink",
    "boldfocus": "Bold Focus",
    "boxarthide": "Hide Box Art",
    "highcontrast": "High Contrast",
    "overlayimage": "Overlay Image",
    "overlaytransparency": "Overlay Transparency",
    "reducemotion": "Reduce Motion",
    "soundfont": "Sound Font",
}

PROFILE_DENIED = {
    "web/ttyd_user",
    "web/ttyd_pass",
    "settings/general/audiosink",
    "settings/general/brightness",
    "settings/general/volume",
    "settings/general/orientation",
    "settings/general/rgb",
    "settings/advanced/part_external",
    "settings/advanced/part_secondary",
    "settings/advanced/passcode",
    "settings/advanced/user_init",
}

CONFIG_FIELD = re.compile(
    r'\{CONF_CONFIG_PATH "([^"]+)", "([^"]+)", CFG_OFF\([^)]*\), (\d+), \{\.([is]) = ("(?:[^"\\]|\\.)*"|-?\d+)\}'
    r'(?:, (\d+), (-?\d+), (-?\d+))?\}'
)
KIOSK_FIELD = re.compile(r'CFG_INT_FIELD\([^,]+, CONF_KIOSK_PATH "([^"]+)", (-?\d+)\)')
LANGUAGE_ENTRY = re.compile(r'\{"[^"]*", LANG_OFF\(([A-Za-z0-9_.\[\]]+)\), \w+, "((?:[^"\\]|\\.)*)"\}')
SAVE_MACRO = re.compile(
    r'CHECK_AND_SAVE_(STD|KSK|VAL|MAP|PCT)\(\s*(\w+),\s*(\w+),\s*"([^"]+)"\s*,?\s*([^;]*?)\);', re.S
)
INIT_OPTION = re.compile(
    r'INIT_OPTION_ITEM\(\s*-?\d+,\s*(\w+),\s*(\w+),\s*lang\.([\w.]+),\s*"([^"]*)",\s*(?:\(char \*\*\)\s*)?([\w]+|NULL),\s*([^)]*)\)', re.S
)
ARRAY_DECL = re.compile(r'(?:static\s+)?(?:const\s+)?char\s*\*\s*(?:const\s+)?(\w+)\[[^\]]*\]\s*=\s*\{([^}]*)\}', re.S)
ARRAY_ASSIGN = re.compile(r'^\s*(\w+)\[(\d+)\]\s*=\s*(lang\.[\w.]+)\s*;', re.M)
INT_ARRAY_DECL = re.compile(r'(?:static\s+)?(?:const\s+)?(?:int|int16_t)\s+(\w+)\[[^\]]*\]\s*=\s*\{([^}]*)\}', re.S)
ENUM_VALUE = re.compile(r'^\s*(\w+)\s*=\s*(-?\d+)\s*,?\s*$', re.M)


def unescape(text):
    return bytes(text, "utf-8").decode("unicode_escape").encode("latin-1").decode("utf-8")


def read(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


def profile_group(key):
    best = None
    for prefix, group, title in PROFILE_GROUPS:
        if key.startswith(prefix + "/") and (best is None or len(prefix) > len(best[0])):
            best = (prefix, group, title)
    return best


def humanise(key):
    leaf = key.rsplit("/", 1)[-1]
    if leaf in LABEL_LEAVES:
        return LABEL_LEAVES[leaf]
    words = [word for word in leaf.split("_") if word]
    return " ".join(LABEL_WORDS.get(word, word[:1].upper() + word[1:]) for word in words)


def refused_reason(key):
    for prefix, reason in REFUSED_REASONS:
        if key == prefix or key.startswith(prefix):
            return reason
    return REFUSED_REASONS[-1][1]


def rule_line(setting, scope=""):
    values = "" if setting.get("open") else ",".join(option["value"] for option in setting.get("options", []))
    if setting["type"] != "int":
        return f"{setting['key']}\ttext\t\t\t{values}\t{scope}"
    return f"{setting['key']}\tint\t{setting.get('min', '')}\t{setting.get('max', '')}\t{values}\t{scope}"


def combo_options(internal, key):
    path = os.path.join(internal, COMBO_SOURCES[key])
    options = []
    for line in read(path).splitlines():
        if "=" not in line:
            continue
        index, combo = line.split("=", 1)
        buttons = [button.replace("_", " ") for button in re.findall(r'"([^"]+)"', combo)]
        if index.strip().isdigit() and buttons:
            options.append({"value": index.strip(), "label": "+".join(buttons)})
    return options


def explicit_options(frontend, strings, key):
    module, labels_name, values_name, extra = EXPLICIT_OPTIONS[key]
    text = read(os.path.join(frontend, "module", f"{module}.c"))
    arrays, _ = parse_arrays(text)
    enums = {name: int(value) for name, value in ENUM_VALUE.findall(text)}
    labels = [resolve_lang(ref, strings) for ref in arrays[labels_name]]
    values = list(range(len(labels)))
    if values_name:
        match = re.search(r"(?:int|int16_t)\s+" + values_name + r"\[[^\]]*\]\s*=\s*\{([^}]*)\}", text, re.S)
        values = [int(item) if re.fullmatch(r"-?\d+", item) else enums[item]
                  for item in (part.strip() for part in match.group(1).split(",")) if item]
    if None in labels or len(values) < len(labels):
        sys.exit(f"Could not resolve options for {key}")
    options = [{"value": str(value), "label": label} for value, label in zip(values, labels)]
    options += [{"value": value, "label": resolve_lang(label, strings) or label} for value, label in extra]
    return options


def website_help(text):
    paragraphs = [part for part in text.split("\n\n")
                  if not part.lstrip().startswith(("Press ", "Select this to open"))]
    return "\n\n".join(paragraphs).strip()


def resolve_label(label, strings):
    if label.startswith("lang."):
        return resolve_lang(label, strings) or label
    return re.sub(r"\{(lang\.[\w.]+)\}", lambda match: resolve_lang(match.group(1), strings) or "", label)


def soundfont_options(internal, strings):
    folder = os.path.join(internal, "share/soundfont")
    names = sorted(name[:-4] for name in os.listdir(folder) if name.lower().endswith(".sf2"))
    return [{"value": "", "label": resolve_lang("lang.muxsoundfont.default_name", strings)}] + [
        {"value": name, "label": name} for name in names]


def wifi_values(setting):
    if setting["key"] == "network/type":
        return "dhcp or static"
    if setting["type"] == "int" and setting.get("min") == 0 and setting.get("max") == 1:
        return "0 or 1"
    if setting["type"] == "int" and "min" in setting:
        return f"{setting['min']} to {setting['max']}"
    return "text"


def oem_only(key):
    return key.startswith(OEM_ONLY_PREFIXES) and key not in OEM_ONLY_EXCLUDED


def allowed(key):
    if key in PROFILE_DENIED:
        return False
    return profile_group(key) is not None


def parse_config(frontend):
    fields = []
    for match in CONFIG_FIELD.finditer(read(os.path.join(frontend, "common/config/config.c"))):
        directory, key, kind, slot, raw, has_range, minimum, maximum = match.groups()
        default = unescape(raw[1:-1]) if slot == "s" else raw
        field = {
            "key": f"{directory}/{key}",
            "type": "text" if int(kind) else "int",
            "default": default,
        }
        if kind == "2":
            field["type"] = "int"
            field["min"], field["max"] = 1, 65535
        if has_range == "1":
            field["min"] = int(minimum)
            field["max"] = int(maximum)
        fields.append(field)

    for match in KIOSK_FIELD.finditer(read(os.path.join(frontend, "common/config/kiosk.c"))):
        fields.append({"key": f"kiosk/{match.group(1)}", "type": "int", "default": match.group(2), "min": 0, "max": 1})

    seen = set()
    unique = []
    for field in fields:
        if field["key"] in seen:
            continue
        seen.add(field["key"])
        unique.append(field)
    return unique


def parse_language(frontend):
    strings = {}
    for match in LANGUAGE_ENTRY.finditer(read(os.path.join(frontend, "common/display/language.c"))):
        strings[match.group(1)] = unescape(match.group(2))
    return strings


def resolve_lang(reference, strings, shared=None):
    reference = reference.strip()
    if len(reference) > 1 and reference[0] == reference[-1] == '"':
        return unescape(reference[1:-1])
    indexed = re.fullmatch(r"(\w+)\[(\d+)\]", reference)
    if indexed and shared and indexed.group(1) in shared:
        items = shared[indexed.group(1)]
        position = int(indexed.group(2))
        return resolve_lang(items[position], strings) if position < len(items) else None
    if reference.startswith("lang."):
        reference = reference[5:]
    return strings.get(reference)


def find_help(strings, label_ref, item, udata):
    language_module = label_ref.split(".")[0]
    for candidate in (item, item.replace("_", ""), udata, udata.replace("_", "")):
        text = strings.get(f"{language_module}.help.{candidate}")
        if text:
            return text
    return None


def parse_arrays(text):
    arrays = {}
    for match in ARRAY_DECL.finditer(text):
        arrays[match.group(1)] = [item.strip() for item in match.group(2).split(",") if item.strip()]
    ints = {}
    for match in INT_ARRAY_DECL.finditer(text):
        values = []
        for item in match.group(2).split(","):
            item = item.strip()
            if re.fullmatch(r"-?\d+", item):
                values.append(int(item))
            elif item:
                values = None
                break
        if values:
            ints[match.group(1)] = values
    return arrays, ints


def shared_arrays(frontend):
    text = read(os.path.join(frontend, "common/display/language.c"))
    arrays, _ = parse_arrays(text)
    assigned = {}
    for name, index, value in ARRAY_ASSIGN.findall(text):
        assigned.setdefault(name, {})[int(index)] = value
    for name, items in assigned.items():
        if sorted(items) == list(range(len(items))):
            arrays.setdefault(name, [items[index] for index in range(len(items))])
    return arrays


def parse_modules(frontend, strings):
    shared = shared_arrays(frontend)
    labels = {}
    module_dir = os.path.join(frontend, "module")
    for name in sorted(os.listdir(module_dir)):
        if not name.endswith(".c"):
            continue
        text = read(os.path.join(module_dir, name))
        arrays, ints = parse_arrays(text)
        ui_path = os.path.join(module_dir, "ui", f"ui_{name}")
        ui_arrays, ui_ints = parse_arrays(read(ui_path)) if os.path.exists(ui_path) else ({}, {})
        items = {}
        for match in INIT_OPTION.finditer(text):
            module, item, label, udata, options, _ = match.groups()
            items[(module, item)] = (label, options, udata)

        for match in SAVE_MACRO.finditer(text):
            kind, module, item, path, rest = match.groups()
            key = f"kiosk/{path}" if kind == "KSK" else path
            if (module, item) not in items:
                continue
            label_ref, options_ref, udata = items[(module, item)]
            entry = {"label": resolve_lang(label_ref, strings), "module": name[:-2]}
            help_text = find_help(strings, label_ref, item, udata)
            if help_text:
                entry["help"] = help_text

            names = arrays.get(options_ref) or shared.get(options_ref)
            option_labels = [resolve_lang(ref, strings, shared) for ref in names] if names else None
            args = [arg.strip() for arg in rest.split(",")]
            if kind in ("VAL", "MAP") and not names:
                source = args[1] if kind == "VAL" else args[0]
                listed = ui_ints.get(source) or ints.get(source)
                if listed:
                    entry["options"] = [{"value": str(value), "label": str(value)} for value in listed]
            if kind == "VAL" and args[0] == "CHAR" and option_labels and all(option_labels):
                words = ui_arrays.get(args[1])
                if words and len(words) == len(option_labels):
                    entry["options"] = [
                        {"value": resolve_lang(word, strings), "label": text_label}
                        for word, text_label in zip(words, option_labels)
                    ]
                option_labels = None
            if option_labels and all(option_labels):
                values = None
                if kind in ("STD", "KSK"):
                    offset = int(args[-1]) if kind == "STD" and re.fullmatch(r"-?\d+", args[-1]) else 0
                    values = [index + offset for index in range(len(option_labels))]
                elif kind in ("VAL", "MAP"):
                    source = args[1] if kind == "VAL" else args[0]
                    values = ui_ints.get(source) or ints.get(source)
                if values and len(values) >= len(option_labels):
                    entry["options"] = [
                        {"value": str(value), "label": text_label}
                        for value, text_label in zip(values, option_labels)
                    ]
            labels.setdefault(key, entry)
    return labels


def tree_path(internal, key):
    if key.startswith("kiosk/"):
        return os.path.join(internal, "kiosk", key[len("kiosk/"):])
    return os.path.join(internal, "config", key)


def ensure_defaults(internal, fields, dry_run):
    created = []
    for field in fields:
        path = tree_path(internal, field["key"])
        if os.path.exists(path):
            continue
        created.append(field["key"])
        if dry_run:
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(field["default"])
        os.chmod(path, 0o755)
    return created


def shipped_value(internal, field):
    path = tree_path(internal, field["key"])
    if not os.path.exists(path):
        return field["default"]
    with open(path, encoding="utf-8", errors="replace") as handle:
        value = handle.read()
    value = value.rstrip("\r\n")
    return value if "\n" not in value else field["default"]


def write_if_changed(path, content):
    if os.path.exists(path) and read(path) == content:
        return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    temporary = path + ".tmp"
    with open(temporary, "w", encoding="utf-8") as handle:
        handle.write(content)
    os.replace(temporary, path)
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--frontend", required=True)
    parser.add_argument("--internal", required=True)
    parser.add_argument("--schema", required=True)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    fields = parse_config(args.frontend)
    if len(fields) < 100:
        sys.exit(f"Only {len(fields)} configuration fields parsed, refusing to continue")

    created = ensure_defaults(args.internal, fields, args.dry_run)
    for key in created:
        print(f"{'Would create' if args.dry_run else 'Created'} default: {key}")

    strings = parse_language(args.frontend)
    labels = parse_modules(args.frontend, strings)

    profile_fields = [field for field in fields if allowed(field["key"])]
    profile_fields.sort(key=lambda field: field["key"])

    lines = [
        "name=MustardOS Defaults",
        "description=Every setting a profile can change, at its shipped default",
        "type=system",
    ]
    schema_settings = []
    for field in profile_fields:
        value = shipped_value(args.internal, field)
        lines.append(f"{field['key']}={value}")
        prefix, group, title = profile_group(field["key"])
        setting = {"key": field["key"], "group": group, "type": field["type"], "default": value}
        if "min" in field:
            setting["min"] = field["min"]
            setting["max"] = field["max"]
        setting.update(labels.get(field["key"], {}))
        setting.setdefault("label", humanise(field["key"]))
        if field["key"] in LABEL_KEYS:
            setting["label"] = resolve_label(LABEL_KEYS[field["key"]], strings)
        if field["key"] in COMBO_SOURCES:
            setting["options"] = combo_options(args.internal, field["key"])
        if field["key"] in EXPLICIT_OPTIONS:
            setting["options"] = explicit_options(args.frontend, strings, field["key"])
        if field["key"] in FIXED_OPTIONS:
            setting["options"] = [{"value": value, "label": resolve_label(label, strings)}
                                  for value, label in FIXED_OPTIONS[field["key"]]]
        if field["key"] == "settings/general/soundfont":
            setting["options"] = soundfont_options(args.internal, strings)
        if field["key"] in OPEN_OPTIONS:
            setting["open"] = 1
        if field["key"] in HELP_KEYS:
            setting["help"] = strings.get(HELP_KEYS[field["key"]]) or setting.get("help")
        if setting.get("help"):
            setting["help"] = website_help(setting["help"])
        if not setting.get("help"):
            setting.pop("help", None)
        if field["key"] in FIXED_RANGES and "min" not in setting:
            setting["min"], setting["max"] = FIXED_RANGES[field["key"]]
        schema_settings.append(setting)

    oem_settings = []
    for field in sorted((field for field in fields if oem_only(field["key"])), key=lambda field: field["key"]):
        setting = {"key": field["key"], "type": field["type"]}
        if "min" in field:
            setting["min"], setting["max"] = field["min"], field["max"]
        if field["key"] == "network/type":
            setting["options"] = [{"value": "0", "label": "DHCP"}, {"value": "1", "label": "Static"}]
        setting["label"] = (WIFI_LABELS.get(field["key"]) or labels.get(field["key"], {}).get("label")
                            or humanise(field["key"]))
        oem_settings.append(setting)

    rules = [rule_line(setting) for setting in schema_settings]
    rules += [rule_line(setting, "oem") for setting in oem_settings]
    refused_keys = sorted({field["key"] for field in fields
                           if not allowed(field["key"]) and not oem_only(field["key"])} | PROFILE_DENIED)
    rules += [f"{key}\trefused\t{refused_reason(key)}\t\t\t" for key in refused_keys]

    default_conf = os.path.join(args.internal, "share/profile/system/default.conf")
    rules_conf = os.path.join(args.internal, "share/profile/system/rules.conf")
    schema = {
        "format": 1,
        "groups": [{"id": group, "title": title} for _, group, title in PROFILE_GROUPS],
        "denied": sorted(PROFILE_DENIED),
        "settings": schema_settings,
        "wifi": [{"name": setting["key"].rsplit("/", 1)[-1], "key": setting["key"], "label": setting["label"],
                  "values": wifi_values(setting)}
                 for setting in sorted(oem_settings, key=lambda entry: (entry["key"].startswith("settings/"),
                                                                        WIFI_ORDER.get(entry["key"], 99),
                                                                        entry["key"]))],
    }
    seen_groups = []
    for group in schema["groups"]:
        if group not in seen_groups:
            seen_groups.append(group)
    schema["groups"] = seen_groups

    if args.dry_run:
        print(f"{len(profile_fields)} profile settings, {sum('label' in s for s in schema_settings)} labelled")
        return

    if write_if_changed(default_conf, "\n".join(lines) + "\n"):
        print(f"Wrote {default_conf}")
    if write_if_changed(rules_conf, "\n".join(rules) + "\n"):
        print(f"Wrote {rules_conf}")
    if write_if_changed(args.schema, json.dumps(schema, indent=2, ensure_ascii=False) + "\n"):
        print(f"Wrote {args.schema}")


if __name__ == "__main__":
    main()
