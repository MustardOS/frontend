#pragma once

#define MUX_EXTRACT_OK      0
#define MUX_EXTRACT_ERR     1
#define MUX_EXTRACT_BLOCKED 2

void extraction_poll(void);

void extract_zip_to_dir_with_progress(const char *filename, const char *output, void (*callback)(char *result));

int extract_zip_to_dir(const char *filename, const char *output);

int extract_file_from_zip(const char *zip_path, const char *file_name, const char *output_path);
