#!/bin/sh

if [ -z "$DEVICE" ]; then
    echo "Error: DEVICE not specified."
    echo "Has to be one of RG35XX | RG35XXPLUS | ARCD"
    exit 1
fi

awk -i inplace '{sub(/^fonts\//, ""); print}' filelist.txt
awk -i inplace '!/ui_events.c/' filelist.txt

make clean

make -j$(nproc) DEVICE=$DEVICE
mv $(basename "$(pwd)") ../bin/.

make clean

rm *.log 2>/dev/null
printf "\n"
