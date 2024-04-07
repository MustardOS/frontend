#!/bin/sh

if [ -z "$DEVICE" ]; then
	echo "Error: DEVICE not specified."
	echo "Has to be one of RG35XX | RG35XXPLUS | ARCD"

	exit 1
fi

PARALLEL=6
CONCURRENCY=${CONCURRENCY:-$PARALLEL}

BUILD_TOTAL=0
BUILD_COUNT=0
CURRENT_DIR=$(pwd)
BUILD_ID=0

mkfifo fifo

WAIT_FOR_JOBS() {
    while [ $(jobs | wc -l) -ge $CONCURRENCY ]; do
        wait -n
    done
}

for MUX in */; do
	if [ -d "$MUX" ]; then
		cd "$CURRENT_DIR/$MUX" || continue

		if [ -f "build.sh" ]; then
			echo "Processing directory: $PWD"

			BUILD_ID=$(($BUILD_ID + 1))
			DEVICE=$DEVICE ./build.sh > "build_${BUILD_ID}.log" 2>&1 &
			BUILD_TOTAL=$(($BUILD_TOTAL + 1))

			echo "Started build $BUILD_ID in $PWD"
			WAIT_FOR_JOBS
		fi

		cd "$CURRENT_DIR" || exit
	fi
done

wait

for i in $(seq 1 $BUILD_TOTAL); do
	if [ -f "build_${i}.log" ] && grep -q "Build successful" "build_${i}.log"; then
		BUILD_COUNT=$(($BUILD_COUNT + 1))
	fi
done

rm -f fifo
notify-send "muX Compilation" "Completed $BUILD_COUNT of $BUILD_TOTAL"
