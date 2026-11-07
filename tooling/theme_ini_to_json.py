#!/usr/bin/env python3
"""Convert a MustardOS theme's INI schemes and layout files into a single theme.json.

The INI files are left in place, so the theme keeps working on older frontends.
"""

import argparse
import json
import os
import re
import sys

KEY_PREFIXES = {
    "navigation": "nav_",
    "keyboard": "osk_",
    "notification": "msg_",
    "charging": "charger_",
    "date": "datetime_",
}

BASE_SCHEMES = ("global", "default")
RESOLUTION = re.compile(r"^\d+x\d+$")


def read_fields(frontend):
    source = open(os.path.join(frontend, "common", "display", "theme.c"), encoding="utf-8").read()
    pattern = re.compile(r'\{"([a-z]+)", "([A-Z0-9_]+)", THEME_OFF\([^)]*\), theme_([a-z]+)')
    fields = {(section, key): kind for section, key, kind in pattern.findall(source)}
    fields[("misc", "CONTENT_WIDTH")] = "int"
    return fields


def stripped_name(section, key):
    name = key.lower()
    prefix = KEY_PREFIXES.get(section, section + "_")
    return name[len(prefix):] if name.startswith(prefix) and len(name) > len(prefix) else name


def nest_paths(tokens_list):
    bare = {tuple(tokens) for tokens in tokens_list}
    counts = {}
    for tokens in tokens_list:
        if len(tokens) > 1:
            counts[tokens[0]] = counts.get(tokens[0], 0) + 1

    paths = {}
    groups = {}
    for tokens in tokens_list:
        head = tokens[0]
        if len(tokens) > 1 and counts.get(head, 0) >= 2 and (head,) not in bare:
            groups.setdefault(head, []).append(tokens[1:])
        else:
            paths[tuple(tokens)] = ("_".join(tokens),)

    for head, rest in groups.items():
        for tail, path in nest_paths(rest).items():
            paths[(head,) + tail] = (head,) + path

    return paths


def canonical_paths(fields):
    by_section = {}
    for section, key in fields:
        by_section.setdefault(section, []).append(key)

    result = {}
    for section, keys in by_section.items():
        tokens = {key: tuple(stripped_name(section, key).split("_")) for key in keys}
        paths = nest_paths(list(set(tokens.values())))
        for key in keys:
            result[(section, key)] = (section,) + paths[tokens[key]]
    return result


def read_ini(path):
    sections = {}
    current = None
    for raw in open(path, encoding="utf-8", errors="replace"):
        line = raw.strip()
        if not line or line[0] in ";#":
            continue
        if line.startswith("[") and line.endswith("]"):
            current = sections.setdefault(line[1:-1].strip(), {})
            continue
        if current is None or "=" not in line:
            continue
        key, value = line.split("=", 1)
        current[key.strip()] = value.strip().strip('"')
    return sections


def typed_value(kind, value):
    try:
        if kind == "hex":
            digits = value.lstrip("#")
            if digits.lower().startswith("0x"):
                digits = digits[2:]
            return "#%06X" % int(digits, 16)
        if kind in ("int", "uint"):
            return int(value)
        if kind == "float":
            return float(value)
    except ValueError:
        return value
    if kind == "str":
        return value
    if re.fullmatch(r"-?\d+", value):
        return int(value)
    return value


def place(tree, path, value):
    node = tree
    for part in path[:-1]:
        node = node.setdefault(part, {})
    node[path[-1]] = value


def scheme_object(ini_sections, fields, paths):
    tree = {}
    for section, values in ini_sections.items():
        for key, value in values.items():
            kind = fields.get((section, key))
            if kind:
                place(tree, paths[(section, key)], typed_value(kind, value))
            else:
                place(tree, (section, key), typed_value("", value))
    return tree


def merge(into, extra):
    for section, values in extra.items():
        into.setdefault(section, {}).update(values)
    return into


def resolve(base, dim, folder, name, extension):
    for directory in ([os.path.join(base, dim, folder)] if dim else []) + [os.path.join(base, folder)]:
        path = os.path.join(directory, name + extension)
        if os.path.isfile(path):
            return path
    return None


def names_in(base, dim, folder, extension):
    names = set()
    for directory in ([os.path.join(base, dim, folder)] if dim else []) + [os.path.join(base, folder)]:
        if os.path.isdir(directory):
            names.update(f[: -len(extension)] for f in os.listdir(directory) if f.endswith(extension))
    return names


def build_document(base, dim, fields, paths):
    combined = {}
    for name in BASE_SCHEMES:
        path = resolve(base, dim, "scheme", name, ".ini")
        if path:
            merge(combined, read_ini(path))

    document = {}
    if combined:
        document["scheme"] = scheme_object(combined, fields, paths)

    modules = {}
    for name in sorted(names_in(base, dim, "scheme", ".ini") - set(BASE_SCHEMES)):
        modules.setdefault(name, {})["scheme"] = scheme_object(
            read_ini(resolve(base, dim, "scheme", name, ".ini")), fields, paths
        )
    for name in sorted(names_in(base, dim, "layout", ".json")):
        with open(resolve(base, dim, "layout", name, ".json"), encoding="utf-8") as handle:
            modules.setdefault(name, {})["layout"] = json.load(handle)
    if modules:
        document["modules"] = modules
    return document


def write_document(path, document):
    if not document:
        return 0
    with open(path, "w", encoding="utf-8") as handle:
        json.dump(document, handle, indent=2)
        handle.write("\n")
    return 1


def find_value(node, name):
    if name in node:
        return True, node[name]
    for index, char in enumerate(name):
        if char != "_":
            continue
        child = node.get(name[:index])
        if isinstance(child, dict):
            found, value = find_value(child, name[index + 1:])
            if found:
                return True, value
    return False, None


def lookup(scheme, section, key):
    group = scheme.get(section)
    if not isinstance(group, dict):
        return False, None
    if key in group:
        return True, group[key]
    return find_value(group, stripped_name(section, key))


def check_document(base, dim, document, fields):
    problems = 0
    for name in BASE_SCHEMES:
        path = resolve(base, dim, "scheme", name, ".ini")
        if not path:
            continue
        for section, values in read_ini(path).items():
            for key in values:
                if (section, key) in fields and not lookup(document.get("scheme", {}), section, key)[0]:
                    print("missing %s %s/%s" % (path, section, key), file=sys.stderr)
                    problems += 1
    return problems


def convert_base(base, fields, paths, check):
    written = problems = 0
    dims = [""] + sorted(d for d in os.listdir(base) if RESOLUTION.match(d) and os.path.isdir(os.path.join(base, d)))
    for dim in dims:
        own = [os.path.join(base, dim, folder) for folder in ("scheme", "layout")]
        if dim and not any(os.path.isdir(folder) and os.listdir(folder) for folder in own):
            continue
        document = build_document(base, dim, fields, paths)
        if check:
            problems += check_document(base, dim, document, fields)
        written += write_document(os.path.join(base, dim, "theme.json"), document)
    return written, problems


def convert_theme(theme, fields, paths, check):
    written, problems = convert_base(theme, fields, paths, check)

    alternates = os.path.join(theme, "alternate")
    if os.path.isdir(alternates):
        for entry in sorted(os.listdir(alternates)):
            path = os.path.join(alternates, entry)
            if os.path.isdir(path):
                more, issues = convert_base(path, fields, paths, check)
                written += more
                problems += issues
            elif entry.endswith(".ini"):
                document = {"scheme": scheme_object(read_ini(path), fields, paths)}
                written += write_document(path[: -len(".ini")] + ".json", document)

    return written, problems


def write_reference(path, fields, paths):
    lines = ["| INI section | INI key | theme.json path |", "|---|---|---|"]
    for section, key in sorted(fields):
        lines.append("| %s | %s | `%s` |" % (section, key, ".".join(paths[(section, key)])))
    with open(path, "w", encoding="utf-8") as handle:
        handle.write("\n".join(lines) + "\n")


def main():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("themes", nargs="*", help="theme folders to convert")
    parser.add_argument("--frontend", default=here, help="frontend repository folder")
    parser.add_argument("--check", action="store_true", help="confirm every INI value can be found in the result")
    parser.add_argument("--reference", help="write a markdown table of every key and its theme.json path")
    args = parser.parse_args()

    fields = read_fields(args.frontend)
    paths = canonical_paths(fields)

    if args.reference:
        write_reference(args.reference, fields, paths)

    total = problems = 0
    for theme in args.themes:
        written, issues = convert_theme(theme, fields, paths, args.check)
        total += written
        problems += issues
        print("%s: wrote %d theme.json file(s)" % (theme, written))

    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
