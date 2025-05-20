#ifndef SPIFFS_UTILS_H
#define SPIFFS_UTILS_H

#include <stddef.h>

void init_spiffs(void);
void list_spiffs_files(const char *base_path);
unsigned char *read_file_from_spiffs(const char *path, size_t *length_out);

#endif // SPIFFS_UTILS_H
