#!/bin/sh

set -euf

DEVICE=${DEVICE:-ARM64_A53}
PLATFORM=${PLATFORM:-unix}
ARCH=${ARCH:-arm64}
BUILD=${BUILD:-test}

XTOOL=${XTOOL:-"$HOME/x-tools"}
XDIR=${XDIR:-${XHOST:-}}

# Pinned because clang format output changes between releases and
# this is the one we currently use... royal we! Meaning me!
FORMAT_VERSION=21.1.8

USAGE() {
	printf "MustardOS Frontend Builder + Cross Compile Tool\n"
	printf "\n"
	printf "%s\n" "Usage:"
	printf "  %s guided setup so no flags to remember\n" "$0"
	printf "  %s [print|generate|database|make [args...]]\n" "$0"
	printf "  %s format [--check] [files...]\n" "$0"
	printf "\n"
	printf "%s\n" "Examples:"
	printf "  %s print\n" "$0"
	printf "  %s generate\n" "$0"
	printf "  %s format\n" "$0"
	printf "  %s format --check\n" "$0"
	printf "  %s format retro/ui/ui_manual.c\n" "$0"
	printf "  %s database -j4\n" "$0"
	printf "  %s make -j4\n" "$0"
	printf "  DEVICE=ARM32 BUILD=release %s make -j4\n" "$0"
	printf "\n"
	printf "%s\n" "Environment:"
	printf "  DEVICE   ARM64 ARM64_A53 ARM64_A53_CRYPTO ARM32 ARM32_A9 X86_64 RISCV64 NATIVE GENERIC\n\n"
	printf "  BUILD    test or release\n"
	printf "  DEBUG    0 quiet, 1 normal, 2 verbose\n"
	printf "  XTOOL    toolchain root, default %s/x-tools\n" "$HOME"
	printf "  XDIR     toolchain directory under XTOOL, detected when unset\n\n"
	printf "  CLANG_FORMAT         clang-format %s binary, installed into .cache when unset\n" "$FORMAT_VERSION"
	printf "  INTERNAL_SCRIPT_DIR  internal scripts used to generate verification data\n"
	printf "  INTERNAL_ROOT        internal repository used to generate profile data and default settings\n"
	printf "  PROFILE_SCHEMA_OUTPUT generated profile schema output path\n"
	printf "  INTERNAL_WEB_DIR     dashboard sources checked against the dashboard language list\n"
	printf "  LANGUAGE_OUTPUT      generated language template output path\n"
	printf "  THIRDPARTY_OUTPUT    generated third party header output path\n"
	printf "  VERIFY_OUTPUT        generated script verification data output path\n"
	printf "\n"
	printf "Keep Shell Variables:\n"
	printf "  . %s\n" "$0"
	printf "\n"

	exit 0
}

# Toolchain directories that hold a usable compiler
LIST_TOOLCHAINS() {
	[ -d "$XTOOL" ] || return 0

	set +f
	set -- "$XTOOL"/*
	set -f

	for DT_DIR in "$@"; do
		[ -d "$DT_DIR" ] || continue

		DT_CC=$(TOOLCHAIN_CC "$DT_DIR") || continue
		TC_TUPLE=$("$DT_CC" -dumpmachine 2>/dev/null || true)
		[ -n "$TC_TUPLE" ] || continue

		printf "%s %s\n" "$(basename "$DT_DIR")" "$TC_TUPLE"
	done
}

ASK_MENU() {
	ASK_TITLE=$1
	ASK_TEXT=$2
	shift 2

	if [ -n "$DIALOG" ]; then
		"$DIALOG" --clear --title "$ASK_TITLE" --menu "$ASK_TEXT" 20 74 10 "$@" 3>&1 1>&2 2>&3
		return $?
	fi

	printf "\n%s\n" "$ASK_TITLE" 1>&2
	MENU_N=0

	for MENU_ITEM in "$@"; do
		MENU_N=$((MENU_N + 1))

		if [ $((MENU_N % 2)) -eq 1 ]; then
			MENU_TAG=$MENU_ITEM
		else
			printf "  %2d) %-30s %s\n" $((MENU_N / 2)) "$MENU_TAG" "$MENU_ITEM" 1>&2
			eval "MENU_OPT$((MENU_N / 2))=\$MENU_TAG"
		fi
	done

	MENU_COUNT=$((MENU_N / 2))
	printf "Select [1-%d]: " "$MENU_COUNT" 1>&2
	read -r MENU_PICK

	[ -n "$MENU_PICK" ] || return 1
	eval "MENU_CHOSEN=\${MENU_OPT$MENU_PICK-}"

	[ -n "$MENU_CHOSEN" ] || return 1

	printf "%s" "$MENU_CHOSEN"
}

ASK_INPUT() {
	ASK_TITLE=$1
	ASK_TEXT=$2
	ASK_DEFAULT=$3

	if [ -n "$DIALOG" ]; then
		"$DIALOG" --clear --title "$ASK_TITLE" --inputbox "$ASK_TEXT" 10 60 "$ASK_DEFAULT" 3>&1 1>&2 2>&3
		return $?
	fi

	printf "\n%s [%s]: " "$ASK_TEXT" "$ASK_DEFAULT" 1>&2
	read -r ASK_VALUE

	printf "%s" "${ASK_VALUE:-$ASK_DEFAULT}"
}

WIZARD() {
	DIALOG=""
	command -v dialog >/dev/null 2>&1 && DIALOG=dialog
	[ -z "$DIALOG" ] && command -v whiptail >/dev/null 2>&1 && DIALOG=whiptail

	WIZ_DEVICE=$(ASK_MENU "Target Device" "Which device is this build for?" \
		ARM64_A53 "H700 and A133P handhelds" \
		ARM64_A53_CRYPTO "As above with hardware crypto" \
		ARM64 "Any other 64 bit ARM" \
		ARM32 "Original 35x series, armhf" \
		ARM32_A9 "Cortex A9 with NEON" \
		X86_64 "64 bit x86" \
		RISCV64 "64 bit RISC-V" \
		NATIVE "Build for this machine") || exit 0

	[ -n "$WIZ_DEVICE" ] || exit 0
	DEVICE=$WIZ_DEVICE

	WIZ_BUILD=$(ASK_MENU "Build Type" "Release strips test only code" \
		release "Shipping build" \
		test "Development build") || exit 0

	[ -n "$WIZ_BUILD" ] || exit 0
	BUILD=$WIZ_BUILD

	WIZ_DEBUG=$(ASK_MENU "Build Output" "How much should the build print?" \
		1 "Normal, show each step" \
		0 "Quiet" \
		2 "Verbose, show every command") || exit 0

	[ -n "$WIZ_DEBUG" ] || exit 0
	DEBUG=$WIZ_DEBUG

	if [ "$DEVICE" != "NATIVE" ]; then
		WIZ_FOUND=$(LIST_TOOLCHAINS)

		if [ -n "$WIZ_FOUND" ]; then
			MENU_COUNT=$(printf "%s\n" "$WIZ_FOUND" | wc -l)

			if [ "$MENU_COUNT" -gt 1 ]; then
				WIZ_ARGS=$(printf "%s\n" "$WIZ_FOUND" | while read -r TC_DIR TC_TUPLE; do
					printf "%s\n%s\n" "$TC_DIR" "$TC_TUPLE"
				done)

				WIZ_IFS=$IFS
				IFS='
'
				# shellcheck disable=SC2086
				set -f
				set -- $WIZ_ARGS
				IFS=$WIZ_IFS

				MENU_PICK=$(ASK_MENU "Toolchain" "More than one toolchain is installed." "$@") || exit 0
				[ -n "$MENU_PICK" ] && XDIR=$MENU_PICK
			fi
		fi
	fi

	WIZ_JOBS=$(ASK_INPUT "Parallel Jobs" "How many compile jobs at once?" "$(nproc 2>/dev/null || echo 4)")
	[ -n "$WIZ_JOBS" ] || WIZ_JOBS=1

	[ -n "$DIALOG" ] && "$DIALOG" --clear --title "Ready" --yesno \
		"Device:   $DEVICE
Build:    $BUILD
Debug:    $DEBUG
Jobs:     $WIZ_JOBS
Toolchain: ${XDIR:-auto}

Start the build?" 14 60 || {
		[ -n "$DIALOG" ] && exit 0
	}

	[ -n "$DIALOG" ] && clear

	export DEVICE BUILD DEBUG XDIR

	printf "Running: DEVICE=%s BUILD=%s DEBUG=%s %s%s make -j%s\n\n" \
		"$DEVICE" "$BUILD" "$DEBUG" \
		"${XDIR:+XDIR=$XDIR }" "$0" "$WIZ_JOBS"

	WIZARD_JOBS=$WIZ_JOBS
}

DEVICE_ARCH() {
	case "$DEVICE" in
		ARM64*) printf "aarch64" ;;
		ARM32*) printf "arm" ;;
		X86_64) printf "x86_64" ;;
		RISCV64) printf "riscv64" ;;
		*) printf "" ;;
	esac
}

TOOLCHAIN_CC() {
	TC_DIR=$1
	[ -d "$TC_DIR/bin" ] || return 1

	set +f
	set -- "$TC_DIR"/bin/*-gcc
	set -f

	TC_FIRST=""

	for TC_GCC in "$@"; do
		[ -x "$TC_GCC" ] || continue
		[ -z "$TC_FIRST" ] && TC_FIRST="$TC_GCC"

		TC_TUPLE=$("$TC_GCC" -dumpmachine 2>/dev/null || true)
		case "$(basename "$TC_GCC")" in
			"$TC_TUPLE-gcc")
				printf "%s" "$TC_GCC"
				return 0
				;;
		esac
	done

	[ -n "$TC_FIRST" ] || return 1
	printf "%s" "$TC_FIRST"
}

DETECT_TOOLCHAIN() {
	DT_WANT=$(DEVICE_ARCH)
	DT_CANDIDATES=""

	if [ -n "$XDIR" ]; then
		DT_CANDIDATES="$XTOOL/$XDIR"
	else
		[ -d "$XTOOL" ] || return 0

		set +f
		set -- "$XTOOL"/*
		set -f

		for DT_DIR in "$@"; do
			[ -d "$DT_DIR" ] && DT_CANDIDATES="$DT_CANDIDATES $DT_DIR"
		done
	fi

	DT_FALLBACK=""

	for DT_DIR in $DT_CANDIDATES; do
		DT_CC=$(TOOLCHAIN_CC "$DT_DIR") || continue
		TC_TUPLE=$("$DT_CC" -dumpmachine 2>/dev/null || true)
		[ -n "$TC_TUPLE" ] || continue

		DT_ARCH=${TC_TUPLE%%-*}
		[ -z "$DT_FALLBACK" ] && DT_FALLBACK="$DT_DIR|$DT_CC|$TC_TUPLE|$DT_ARCH"

		if [ -z "$DT_WANT" ] || [ "$DT_ARCH" = "$DT_WANT" ]; then
			XDIR=$(basename "$DT_DIR")
			XBIN="$DT_DIR/bin"
			XHOST="$TC_TUPLE"
			XARCH="$DT_ARCH"
			CROSS_COMPILE="${DT_CC%gcc}"

			export XDIR XBIN XHOST XARCH CROSS_COMPILE
			return 0
		fi
	done

	if [ -n "$DT_FALLBACK" ]; then
		XDIR=$(basename "${DT_FALLBACK%%|*}")
		DT_REST=${DT_FALLBACK#*|}
		DT_CC=${DT_REST%%|*}
		DT_REST=${DT_REST#*|}
		XHOST=${DT_REST%%|*}
		XARCH=${DT_REST#*|}
		XBIN=$(dirname "$DT_CC")
		CROSS_COMPILE="${DT_CC%gcc}"

		export XDIR XBIN XHOST XARCH CROSS_COMPILE

		if [ -n "$DT_WANT" ] && [ "$XARCH" != "$DT_WANT" ]; then
			printf "Warning: %s wants a %s toolchain but only %s was found\n" \
				"$DEVICE" "$DT_WANT" "$XARCH" 1>&2
		fi
	fi
}

DETECT_SYSROOT() {
	[ -n "${SYSROOT-}" ] && [ "$SYSROOT" != "" ] && return 0

	SYSROOT=$("${CROSS_COMPILE}gcc" -print-sysroot 2>/dev/null || true)

	if [ -n "$SYSROOT" ] && [ -d "$SYSROOT" ]; then
		export SYSROOT
		return 0
	fi

	C1="$XTOOL/$XDIR/$XHOST/sysroot"
	C2="$XTOOL/$XDIR/$XHOST"

	if [ -d "$C1" ]; then
		SYSROOT="$C1"
	elif [ -d "$C2/sysroot" ]; then
		SYSROOT="$C2/sysroot"
	elif [ -d "$C2/usr" ]; then
		SYSROOT="$C2"
	else
		SYSROOT=""
	fi

	export SYSROOT
}

CPU_CFLAGS_FOR_ARCH() {
	case "$1" in
		aarch64) printf -- '-march=armv8-a' ;;
		arm) printf -- '-march=armv7-a' ;;
		x86_64) printf -- '-march=x86-64-v2' ;;
		riscv64) printf -- '-march=rv64gc' ;;
		*) printf "" ;;
	esac
}

CHECK_TOOLS() {
	if [ ! -d "$XBIN" ]; then
		printf "Error: XBIN not found: %s\n" "$XBIN" 1>&2
		exit 1
	fi

	for T in "${CROSS_COMPILE}gcc" "${CROSS_COMPILE}g++" "${CROSS_COMPILE}ar" "${CROSS_COMPILE}strip"; do
		if ! command -v "$T" >/dev/null 2>&1; then
			printf "Error: required tool not found in PATH: %s\n" "$T" 1>&2
			exit 1
		fi
	done

	if [ -x "$XBIN/pkgconf" ]; then
		PKG_CONFIG="$XBIN/pkgconf"
	elif command -v pkgconf >/dev/null 2>&1; then
		PKG_CONFIG=pkgconf
	else
		PKG_CONFIG=pkg-config
	fi

	export PKG_CONFIG
}

GEN_INSTALL() {
	GEN_TMP=$1
	GEN_OUT=$2

	if [ -f "$GEN_OUT" ] && cmp -s "$GEN_OUT" "$GEN_TMP"; then
		rm -f "$GEN_TMP"
		return 0
	fi

	mv "$GEN_TMP" "$GEN_OUT"
	printf 'Wrote %s\n' "$GEN_OUT"
}

GEN_LANGUAGE() {
	GEN_SRC="$FRONTEND_DIR/common/display/language.c"
	GEN_WEB="$FRONTEND_DIR/common/display/language_web.txt"
	GEN_OUT=${LANGUAGE_OUTPUT:-"$FRONTEND_DIR/common/generated/language.json"}
	mkdir -p "$(dirname "$GEN_OUT")"
	GEN_TMP="$GEN_OUT.tmp"

	[ -f "$GEN_SRC" ] || {
		printf 'Error: language source not found: %s\n' "$GEN_SRC" 1>&2
		exit 1
	}

	[ -f "$GEN_WEB" ] || {
		printf 'Error: dashboard language list not found: %s\n' "$GEN_WEB" 1>&2
		exit 1
	}

	awk -v web="$GEN_WEB" '
        FILENAME == web {
            str = $0
            sub(/\r$/, "", str)
            if (str == "") next
            gsub(/\\/, "\\\\", str)
            gsub(/"/, "\\\"", str)
            if (seen["muweb" SUBSEP str]++) next
            keys["muweb"] = keys["muweb"] str "\n"
            if (!("muweb" in modseen)) { modorder[++nmod] = "muweb"; modseen["muweb"] = 1 }
            next
        }
        /^static const lang_field lang_fields\[\] = \{/ { intable = 1; next }
        intable && /^\/\/ clang-format on/ { intable = 0 }
        !intable { next }
        !/^    \{/ { next }
        {
            raw = $0
            split($0, f, "\"")
            module = f[2]
            type = f[3]
            str = f[4]

            start = index(raw, "LANG_OFF(")
            rest = substr(raw, start + 9)
            finish = index(rest, ")")
            member = substr(rest, 1, finish - 1)
            if (!start || !finish || module == "" || member == "" || str == "") {
                printf "Invalid language field: %s\n", raw > "/dev/stderr"
                errors++
                next
            }
            if (member_seen[member]++) {
                printf "Duplicate language destination: %s\n", member > "/dev/stderr"
                errors++
                next
            }

            sub(/^, LANG_OFF\([^)]*\), /, "", type)
            sub(/,[ \t]*$/, "", type)

            if (type == "lang_system") next
            if (seen[module SUBSEP str]++) next

            keys[module] = keys[module] str "\n"
            if (!(module in modseen)) { modorder[++nmod] = module; modseen[module] = 1 }
        }
        END {
            if (errors) exit 1
            for (i = 1; i <= nmod; i++) msort[i] = modorder[i]
            for (i = 2; i <= nmod; i++) {
                v = msort[i]; j = i - 1
                while (j >= 1 && msort[j] > v) { msort[j + 1] = msort[j]; j-- }
                msort[j + 1] = v
            }

            printf "{\n"
            for (i = 1; i <= nmod; i++) {
                m = msort[i]

                delete sortk
                nk = split(keys[m], arr, "\n")
                realn = 0
                for (k = 1; k <= nk; k++) if (arr[k] != "") sortk[++realn] = arr[k]
                for (a = 2; a <= realn; a++) {
                    v = sortk[a]; b = a - 1
                    while (b >= 1 && sortk[b] > v) { sortk[b + 1] = sortk[b]; b-- }
                    sortk[b + 1] = v
                }

                printf "  \"%s\": {\n", m
                for (k = 1; k <= realn; k++) {
                    printf "    \"%s\": \"%s\"%s\n", sortk[k], sortk[k], (k < realn ? "," : "")
                }
                printf "  }%s\n", (i < nmod ? "," : "")

                delete arr
            }
            printf "}\n"
        }
    ' "$GEN_WEB" "$GEN_SRC" >"$GEN_TMP"

	GEN_INSTALL "$GEN_TMP" "$GEN_OUT"
	GEN_CHECK_WEB
}

GEN_CHECK_WEB() {
	GEN_WEB_DIR=${INTERNAL_WEB_DIR:-"$FRONTEND_DIR/../internal/share/web"}
	[ -d "$GEN_WEB_DIR/js" ] || return 0

	find "$GEN_WEB_DIR/js" -name '*.js' -exec grep -ohE '(^|[^A-Za-z0-9_$.])t\("[^"]*"' {} + | sed 's/^.*t("//; s/"$//' | sort -u | while IFS= read -r GEN_STRING; do
		grep -qxF -- "$GEN_STRING" "$GEN_WEB" ||
			printf 'Warning: dashboard string missing from %s: %s\n' "$GEN_WEB" "$GEN_STRING" 1>&2
	done
}

GEN_EXTERNAL_VERSION() {
	sed -n 's/^VERSION="\([^"]*\)".*/\1/p' "$FRONTEND_DIR/external/$1.sh" | head -1
}

GEN_MACRO_VERSION() {
	GEN_FILE=$1
	shift
	GEN_VERSION=""

	for GEN_MACRO in "$@"; do
		GEN_PART=$(sed -n "s/^#define[[:space:]]*${GEN_MACRO}[[:space:]]*\([0-9][0-9]*\).*/\1/p" "$GEN_FILE" | head -1)
		[ -n "$GEN_PART" ] || return 0
		GEN_VERSION="${GEN_VERSION:+$GEN_VERSION.}$GEN_PART"
	done

	printf '%s' "$GEN_VERSION"
}

GEN_STRING_VERSION() {
	sed -n "s/^#define[[:space:]]*$2[[:space:]]*\"\([^\"]*\)\".*/\1/p" "$1" | head -1
}

GEN_THIRDPARTY() {
	GEN_OUT=${THIRDPARTY_OUTPUT:-"$FRONTEND_DIR/common/generated/thirdparty.h"}
	mkdir -p "$(dirname "$GEN_OUT")"
	GEN_TMP="$GEN_OUT.tmp"

	GEN_ROWS=$(
		printf '%s\t%s\n' "FFmpeg" "$(GEN_EXTERNAL_VERSION ffmpeg)"
		printf '%s\t%s\n' "json.c" ""
		printf '%s\t%s\n' "libarchive" "$(GEN_EXTERNAL_VERSION libarchive)"
		printf '%s\t%s\n' "libretro" "$(GEN_MACRO_VERSION "$FRONTEND_DIR/retro/core/libretro.h" RETRO_API_VERSION)"
		printf '%s\t%s\n' "LVGL" "$(GEN_MACRO_VERSION "$FRONTEND_DIR/vendor/lvgl/lvgl.h" LVGL_VERSION_MAJOR LVGL_VERSION_MINOR LVGL_VERSION_PATCH)"
		printf '%s\t%s\n' "mdns" "a569c475"
		printf '%s\t%s\n' "minic" ""
		printf '%s\t%s\n' "miniz" "$(GEN_STRING_VERSION "$FRONTEND_DIR/vendor/miniz/miniz.h" MZ_VERSION)"
		printf '%s\t%s\n' "Mojibake" "$(GEN_EXTERNAL_VERSION mojibake)"
		printf '%s\t%s\n' "OpenSSL" "$(GEN_EXTERNAL_VERSION openssl)"
		printf '%s\t%s\n' "PlutoSVG" "$(GEN_MACRO_VERSION "$FRONTEND_DIR/vendor/plutosvg/source/plutosvg.h" PLUTOSVG_VERSION_MAJOR PLUTOSVG_VERSION_MINOR PLUTOSVG_VERSION_MICRO)"
		printf '%s\t%s\n' "PlutoVG" "$(GEN_MACRO_VERSION "$FRONTEND_DIR/vendor/plutosvg/plutovg/include/plutovg.h" PLUTOVG_VERSION_MAJOR PLUTOVG_VERSION_MINOR PLUTOVG_VERSION_MICRO)"
		printf '%s\t%s\n' "rcheevos" "$(GEN_EXTERNAL_VERSION rcheevos)"
		printf '%s\t%s\n' "stb_image_write" "1.16"
		printf '%s\t%s\n' "stb_rect_pack" "1.01"
		printf '%s\t%s\n' "stb_truetype" "1.26"
		printf '%s\t%s\n' "xxHash" "$(GEN_MACRO_VERSION "$FRONTEND_DIR/vendor/xxhash/xxhash.h" XXH_VERSION_MAJOR XXH_VERSION_MINOR XXH_VERSION_RELEASE)"
	)

	{
		echo "#pragma once"
		echo
		echo "// Generated by build.sh - do not edit"
		echo
		echo "struct third_party_lib {"
		echo "    const char *name;"
		echo "    const char *version;"
		echo "};"
		echo
		echo "static const struct third_party_lib third_party_libs[] = {"

		printf '%s\n' "$GEN_ROWS" | LC_ALL=C sort -f | while IFS="$(printf '\t')" read -r GEN_NAME GEN_VERSION; do
			[ -n "$GEN_NAME" ] || continue
			[ -n "$GEN_VERSION" ] || GEN_VERSION="-"
			printf '    {"%s", "%s"},\n' "$GEN_NAME" "$GEN_VERSION"
		done

		echo "};"
	} >"$GEN_TMP"

	GEN_INSTALL "$GEN_TMP" "$GEN_OUT"
}

GEN_VERIFY() {
	GEN_INTERNAL=${INTERNAL_SCRIPT_DIR:-"$FRONTEND_DIR/../internal/script"}
	GEN_OUT=${VERIFY_OUTPUT:-"$FRONTEND_DIR/common/generated/verify_data.h"}
	GEN_TARGET_BASE="/opt/muos/script"
	GEN_DIRS="archive control device init launch mount mux package system var web"

	if [ ! -d "$GEN_INTERNAL" ]; then
		[ -f "$GEN_OUT" ] || {
			printf 'Error: internal script directory not found: %s\n' "$GEN_INTERNAL" 1>&2
			exit 1
		}
		printf 'Using existing verification data; internal scripts not found at %s\n' "$GEN_INTERNAL"
		return 0
	fi

	command -v xxhsum >/dev/null 2>&1 || {
		printf '%s\n' "Error: xxhsum is required to generate script verification data" 1>&2
		exit 1
	}

	GEN_INTERNAL=${GEN_INTERNAL%/}
	mkdir -p "$(dirname "$GEN_OUT")"
	GEN_TMP="$GEN_OUT.tmp"

	{
		echo "#pragma once"
		echo
		echo "// Generated by build.sh - do not edit"
		echo
		echo "struct int_script_hash {"
		echo "    const char *path;"
		echo "    const char *hash;"
		echo "};"
		echo
		echo "static const struct int_script_hash int_scripts[] = {"

		for GEN_DIR in $GEN_DIRS; do
			[ -d "$GEN_INTERNAL/$GEN_DIR" ] || continue
			find "$GEN_INTERNAL/$GEN_DIR" -type f
		done |
			LC_ALL=C sort |
			while IFS= read -r GEN_FILE; do
				GEN_HASH_LINE=$(xxhsum <"$GEN_FILE")
				GEN_HASH=${GEN_HASH_LINE%% *}
				[ -n "$GEN_HASH" ] || exit 1
				GEN_TARGET_PATH=$(printf '%s\n' "$GEN_FILE" | sed "s|^$GEN_INTERNAL|$GEN_TARGET_BASE|")
				printf "    { \"%s\", \"%s\" },\n" "$GEN_TARGET_PATH" "$GEN_HASH"
			done

		echo "};"
	} >"$GEN_TMP"

	GEN_INSTALL "$GEN_TMP" "$GEN_OUT"
}

GEN_PROFILE() {
	GEN_INTERNAL_ROOT=${INTERNAL_ROOT:-"$FRONTEND_DIR/../internal"}
	GEN_OUT=${PROFILE_SCHEMA_OUTPUT:-"$FRONTEND_DIR/common/generated/profile_schema.json"}

	if [ ! -d "$GEN_INTERNAL_ROOT/config" ]; then
		printf 'Skipping profile data; internal repository not found at %s\n' "$GEN_INTERNAL_ROOT"
		return 0
	fi

	command -v python3 >/dev/null 2>&1 || {
		printf '%s\n' "Error: python3 is required to generate profile data" 1>&2
		exit 1
	}

	python3 "$FRONTEND_DIR/tooling/profile_generate.py" --frontend "$FRONTEND_DIR" \
		--internal "$GEN_INTERNAL_ROOT" --schema "$GEN_OUT" || exit 1
}

GENERATE() {
	FRONTEND_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
	GEN_LANGUAGE
	GEN_THIRDPARTY
	GEN_PROFILE
	GEN_VERIFY
}

COMPILE_DATABASE() {
	DB_CC=$1
	shift

	FRONTEND_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
	DB_CAPTURE_DIR=/tmp/mustardos/compile-database
	DB_OUTPUT="$FRONTEND_DIR/compile_commands.json"
	DB_CAPTURE="$FRONTEND_DIR/tooling/compdb"

	rm -rf "$DB_CAPTURE_DIR"
	mkdir -p "$DB_CAPTURE_DIR"
	export MUOS_COMPDB_DIR="$DB_CAPTURE_DIR"
	export MUOS_COMPDB_ROOT="$FRONTEND_DIR"

	make BUILD="$BUILD" clean
	make BUILD="$BUILD" CC="$DB_CAPTURE ccache $DB_CC" "$@"
	"$DB_CAPTURE" --merge "$DB_OUTPUT" "$DB_CAPTURE_DIR"

	printf 'Wrote %s\n' "$DB_OUTPUT"
}

FORMAT_IS_PINNED() {
	[ -n "$1" ] || return 1
	"$1" --version 2>/dev/null | grep -q "clang-format version $FORMAT_VERSION\( \|$\)"
}

# Use CLANG_FORMAT or a matching one on PATH, otherwise install the pinned release into .cache
FIND_CLANG_FORMAT() {
	FORMAT_ENV="$FRONTEND_DIR/.cache/clang-format-$FORMAT_VERSION"
	FORMAT_BIN="$FORMAT_ENV/bin/clang-format"

	if [ -n "${CLANG_FORMAT-}" ]; then
		if FORMAT_IS_PINNED "$CLANG_FORMAT"; then
			FORMAT_BIN=$CLANG_FORMAT
			return 0
		fi

		printf "Error: CLANG_FORMAT=%s is not clang-format %s\n" "$CLANG_FORMAT" "$FORMAT_VERSION" 1>&2
		exit 1
	fi

	FORMAT_IS_PINNED "$FORMAT_BIN" && return 0

	for FORMAT_TRY in clang-format-21 clang-format; do
		FORMAT_PATH=$(command -v "$FORMAT_TRY" 2>/dev/null || true)
		if FORMAT_IS_PINNED "$FORMAT_PATH"; then
			FORMAT_BIN=$FORMAT_PATH
			return 0
		fi
	done

	if ! command -v python3 >/dev/null 2>&1; then
		printf "Error: clang-format %s not found and python3 is needed to install it\n" "$FORMAT_VERSION" 1>&2
		printf "  Set CLANG_FORMAT to a clang-format %s binary instead\n" "$FORMAT_VERSION" 1>&2
		exit 1
	fi

	printf "Installing clang-format %s into %s\n" "$FORMAT_VERSION" "$FORMAT_ENV"
	rm -rf "$FORMAT_ENV"

	if ! python3 -m venv "$FORMAT_ENV" ||
		! "$FORMAT_ENV/bin/python3" -m pip install -q --disable-pip-version-check "clang-format==$FORMAT_VERSION"; then
		rm -rf "$FORMAT_ENV"
		printf "Error: could not install clang-format %s with python3 venv and pip\n" "$FORMAT_VERSION" 1>&2
		exit 1
	fi

	if ! FORMAT_IS_PINNED "$FORMAT_BIN"; then
		printf "Error: installed clang-format does not report version %s\n" "$FORMAT_VERSION" 1>&2
		exit 1
	fi
}

# Tracked sources only, clang-format skips anything listed in .clang-format-ignore
FORMAT_SOURCES() {
	git -C "$FRONTEND_DIR" ls-files -- '*.c' '*.h'
}

FORMAT() {
	FRONTEND_DIR=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)

	FORMAT_CHECK=0
	if [ "${1-}" = "--check" ]; then
		FORMAT_CHECK=1
		shift
	fi

	FIND_CLANG_FORMAT

	FORMAT_LIST=$(mktemp)
	trap 'rm -f "$FORMAT_LIST"' EXIT

	if [ $# -gt 0 ]; then
		printf "%s\n" "$@" >"$FORMAT_LIST"
	else
		cd "$FRONTEND_DIR"
		FORMAT_SOURCES >"$FORMAT_LIST"
	fi

	FORMAT_BAD=0
	FORMAT_DONE=0

	while IFS= read -r FORMAT_FILE; do
		[ -n "$FORMAT_FILE" ] || continue

		if [ ! -f "$FORMAT_FILE" ]; then
			printf "Error: %s not found\n" "$FORMAT_FILE" 1>&2
			exit 1
		fi

		if [ "$FORMAT_CHECK" -eq 1 ]; then
			if ! "$FORMAT_BIN" --dry-run -Werror "$FORMAT_FILE" >/dev/null 2>&1; then
				printf "Needs formatting: %s\n" "$FORMAT_FILE"
				FORMAT_BAD=$((FORMAT_BAD + 1))
			fi
		else
			# Rewriting in place drops the executable bit the sources are tracked with
			FORMAT_EXEC=0
			[ ! -x "$FORMAT_FILE" ] || FORMAT_EXEC=1

			"$FORMAT_BIN" -i "$FORMAT_FILE"

			[ "$FORMAT_EXEC" -eq 0 ] || chmod +x "$FORMAT_FILE"
		fi

		FORMAT_DONE=$((FORMAT_DONE + 1))
	done <"$FORMAT_LIST"

	if [ "$FORMAT_CHECK" -eq 1 ]; then
		printf "Checked %d files with clang-format %s, %d need formatting\n" "$FORMAT_DONE" "$FORMAT_VERSION" "$FORMAT_BAD"
		[ "$FORMAT_BAD" -eq 0 ] || exit 1
	else
		printf "Formatted %d files with clang-format %s\n" "$FORMAT_DONE" "$FORMAT_VERSION"
	fi
}

if [ "${1-}" = "generate" ]; then
	shift
	[ $# -eq 0 ] || USAGE
	GENERATE
	exit 0
fi

if [ "${1-}" = "format" ]; then
	shift
	FORMAT "$@"
	exit 0
fi

WIZARD_JOBS=""
if [ $# -eq 0 ] && [ -t 0 ] && [ -t 1 ]; then
	WIZARD
	set -- make -j"$WIZARD_JOBS"
fi

if [ "$DEVICE" = "NATIVE" ]; then
	unset CC CXX AR LD STRIP CROSS_COMPILE
	unset CFLAGS CPPFLAGS CXXFLAGS LDFLAGS
	unset INC_DIR LIB_DIR
	unset PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_LIBDIR SYSROOT

	export DEVICE BUILD

	CMD=${1-}
	case "$CMD" in
		database)
			shift
			COMPILE_DATABASE gcc "$@"
			;;
		make)
			shift
			if command -v make >/dev/null 2>&1; then
				exec make BUILD="$BUILD" "$@"
			fi
			printf "%s\n" "Error: 'make' not found on PATH." 1>&2
			exit 1
			;;
		print)
			printf "Device:        %s\n" "$DEVICE"
			printf "Build:         %s\n" "$BUILD"
			printf "CC:            ccache gcc (native host)\n"
			;;
		*) USAGE ;;
	esac

	exit $?
fi

DETECT_TOOLCHAIN

if [ -z "${CROSS_COMPILE-}" ]; then
	printf "Error: no usable toolchain found under %s\n" "$XTOOL" 1>&2
	printf "  Set XDIR to a directory name under it, or XTOOL to another root.\n" 1>&2
	exit 1
fi

PATH="$XBIN:$PATH"
export PATH

DETECT_SYSROOT

if [ -z "$SYSROOT" ]; then
	printf "Warning: could not detect SYSROOT for %s\n" "$XDIR" 1>&2
fi

CC="${CROSS_COMPILE}gcc"
CXX="${CROSS_COMPILE}g++"
AR="${CROSS_COMPILE}ar"
LD="${CROSS_COMPILE}ld"
STRIP="${CROSS_COMPILE}strip"
export CC CXX AR LD STRIP

CPPFLAGS=${CPPFLAGS:-}
CFLAGS=${CFLAGS:-$(CPU_CFLAGS_FOR_ARCH "$XARCH")}
CXXFLAGS=${CXXFLAGS:-}
LDFLAGS=${LDFLAGS:-}

if [ -n "$SYSROOT" ]; then
	CPPFLAGS="${CPPFLAGS} --sysroot=$SYSROOT -I$SYSROOT/usr/include"
	CXXFLAGS="${CXXFLAGS} --sysroot=$SYSROOT -I$SYSROOT/usr/include"
	CFLAGS="${CFLAGS} --sysroot=$SYSROOT -I$SYSROOT/usr/include"
	LDFLAGS="${LDFLAGS} --sysroot=$SYSROOT -L$SYSROOT/lib -L$SYSROOT/usr/lib -L$SYSROOT/usr/local/lib"
fi

export CPPFLAGS CFLAGS CXXFLAGS LDFLAGS

# pkg-config stuff - tell it about the sysroot and where .pc files live - this is annoying
if [ -n "$SYSROOT" ]; then
	PKG_CONFIG_SYSROOT_DIR="$SYSROOT"
	PKG_CONFIG_LIBDIR="$SYSROOT/usr/lib/pkgconfig:$SYSROOT/usr/share/pkgconfig:$SYSROOT/usr/local/lib/pkgconfig"
	export PKG_CONFIG_SYSROOT_DIR PKG_CONFIG_LIBDIR
fi

ARMABI="$XHOST"
TOOLCHAIN_DIR="$XTOOL/$XDIR"
DESTDIR="${SYSROOT:-}"
ARCH="$XARCH"
export DEVICE PLATFORM ARCH ARMABI TOOLCHAIN_DIR DESTDIR BUILD

INC_DIR="$CPPFLAGS"
LIB_DIR="$LDFLAGS"
export INC_DIR LIB_DIR

if command -v "${PKG_CONFIG:-pkg-config}" >/dev/null 2>&1; then
	if "${PKG_CONFIG:-pkg-config}" --exists sdl2 >/dev/null 2>&1; then
		SDL2CONFIG="$SYSROOT/usr/bin/sdl2-config"
		export SDL2CONFIG
	fi
	if "${PKG_CONFIG:-pkg-config}" --exists alsa >/dev/null 2>&1; then
		ALSA_CFLAGS=$("${PKG_CONFIG:-pkg-config}" --cflags alsa 2>/dev/null || printf "")
		ALSA_LIBS=$("${PKG_CONFIG:-pkg-config}" --libs alsa 2>/dev/null || printf "")
		export ALSA_CFLAGS ALSA_LIBS
	fi
fi

CHECK_TOOLS

PRINT_ENV() {
	printf "Device:        %s\n" "$DEVICE"
	printf "Platform:      %s\n" "$PLATFORM"
	printf "Arch:          %s\n" "$ARCH"
	printf "Build:         %s\n" "$BUILD"
	printf "Cross Root:    %s\n" "$XTOOL"
	printf "Host Tuple:    %s\n" "$XHOST"
	printf "Bin Path:      %s\n" "$XBIN"

	if [ -n "$SYSROOT" ]; then
		printf "Sysroot:       %s\n" "$SYSROOT"
	else
		printf "Sysroot:       (not detected)\n"
	fi

	printf "CC:            %s\n" "$CC"
	printf "CFLAGS:        %s\n" "$CFLAGS"
	printf "CXXFLAGS:      %s\n" "$CXXFLAGS"
	printf "CPPFLAGS:      %s\n" "$CPPFLAGS"
	printf "LDFLAGS:       %s\n" "$LDFLAGS"

	if [ -n "${PKG_CONFIG-}" ]; then
		printf "PKG_CONFIG:    %s\n" "$PKG_CONFIG"
	fi
}

RUN_MAKE() {
	if command -v make >/dev/null 2>&1; then
		exec make BUILD="$BUILD" "${@-}"
	fi

	printf "%s\n" "Error: 'make' not found on PATH." 1>&2
	exit 1
}

CMD=${1-}
case "$CMD" in
	database) shift && COMPILE_DATABASE "$CC" "$@" ;;
	make) shift && RUN_MAKE "$@" ;;
	print) PRINT_ENV ;;
	*) USAGE ;;
esac
