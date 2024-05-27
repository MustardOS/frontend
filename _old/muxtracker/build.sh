#!/bin/sh

if [ -z "$DEVICE" ]; then
    echo "Error: DEVICE not specified"
    exit 1
fi

# Workaround to remove blue outlines from basic LVGL themes
awk '/lv_disp_t \* dispp = lv_disp_get_default\(\)/ {print NR}' ui/ui.c | while read -r line_num; do sed -i "${line_num},$((line_num+3))d" ui/ui.c; done

# As we are using a flat file system we need to adjust for this
awk -i inplace '{sub(/^fonts\//, ""); print}' ui/filelist.txt
awk -i inplace '!/ui_events.c/' ui/filelist.txt

# Add the 'ui' directory...
sed -i 's|^|ui/|' ui/filelist.txt

make clean

make -j$(nproc) DEVICE=$DEVICE
mv $(basename "$(pwd)") ../bin/.

make clean

# Remove the 'ui' directory...
sed -i 's|^ui/||' ui/filelist.txt
