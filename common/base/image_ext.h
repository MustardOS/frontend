#pragma once

int is_image_ext(const char *ext);

const char **image_ext_list(int *count);

int is_overlay_ext(const char *ext);

const char **overlay_ext_list(int *count);
