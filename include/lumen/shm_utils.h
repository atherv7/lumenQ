#ifndef LUMEN_SHM_UTILS_H
#define LUMEN_SHM_UTILS_H

#include <stddef.h>

void *lumen_shm_map(const char *path, size_t size, int is_producer,
                    int *out_fd);

void lumen_shm_unmap(void *mapped_ptr, size_t size, int fd, const char *path,
                     int is_producer);

#endif // LUMEN_SHM_UTILS_J
