#!/bin/sh

if [ -z "$DEVICE" ]; then
	echo "Error: DEVICE not specified"
	exit 1
fi

BUILD_TOTAL=0
BUILD_COUNT=0
CURRENT_DIR=$(pwd)

rm -rf bin/mux*

for MUX in */; do
	if [ -d "$MUX" ]; then
		cd "$CURRENT_DIR/$MUX" || continue
		echo "Processing directory: $PWD"
		if [ -f "build.sh" ]; then
			echo "Executing build.sh in $PWD"
			BUILD_TOTAL=$((BUILD_TOTAL + 1))
			DEVICE=$DEVICE ./build.sh && BUILD_COUNT=$((BUILD_COUNT + 1))
		else
			echo "build.sh not found in $PWD"
		fi
		cd "$CURRENT_DIR" || exit
	fi
done

notify-send "muOS Frontend" "Completed $BUILD_COUNT of $BUILD_TOTAL" && echo -e '\a'

