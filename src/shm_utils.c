#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

void *lumen_shm_map(const char *path, size_t size, int is_producer,
                    int *out_fd) {
  int oflag = is_producer ? (O_CREAT | O_RDWR | O_TRUNC) : O_RDWR;

  int fd = shm_open(path, oflag, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    perror("shm_open failed");
    return NULL;
  }

  if (is_producer) {
    if (ftruncate(fd, size) == -1) {
      perror("ftruncate failed");
      close(fd);
      shm_unlink(path);
      return NULL;
    }
  }

  void *mapped_ptr =
      mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_ptr == MAP_FAILED) {
    perror("mmap failed");
    close(fd);
    if (is_producer) {
      shm_unlink(path);
    }
    return NULL;
  }

  *out_fd = fd;
  return mapped_ptr;
}

void lumen_shm_unmap(void *mapped_ptr, size_t size, int fd, const char *path,
                     int is_producer) {
  if (mapped_ptr && mapped_ptr != MAP_FAILED) {
    munmap(mapped_ptr, size);
  }

  if (fd >= 0) {
    close(fd);
  }

  if (is_producer && path) {
    shm_unlink(path);
  }
}
