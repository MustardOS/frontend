#!/bin/sh

set -eu

HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
J2ME_JAR=${1:-"$HERE/../../../internal/init/MUOS/bios/freej2me-lr.jar"}
JAVA=${JAVA:-java}

if [ ! -f "$J2ME_JAR" ]; then
	printf 'Error: freej2me-lr.jar not found at %s\n' "$J2ME_JAR" 1>&2
	printf 'Usage: %s [path/to/freej2me-lr.jar]\n' "$0" 1>&2
	exit 1
fi

BUILD_DIR=$(mktemp -d)
trap 'rm -rf "$BUILD_DIR"' EXIT

"$JAVA" -m jdk.compiler/com.sun.tools.javac.Main -source 8 -target 8 -nowarn -Xlint:-options \
	-cp "$J2ME_JAR" -d "$BUILD_DIR" "$HERE/PicklesTest.java"
"$JAVA" -m jdk.jartool/sun.tools.jar.Main cfm "$HERE/PicklesTest.jar" "$HERE/manifest.txt" -C "$BUILD_DIR" .

printf 'Wrote %s\n' "$HERE/PicklesTest.jar"
