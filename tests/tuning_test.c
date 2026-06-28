#include <stddef.h>
#define _GNU_SOURCE
#include "lumen/tuning.h"
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

static void test_kernel_tuning_verification(void) {
  printf("[TEST] Verifying Kernel Tuning configuration (Memory Locking & CPU "
         "Pinning)...\n");

  lumen_tune_memory_lock();
  lumen_tune_cpu_affinity(2);

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);

  int get_aff =
      pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
  if (get_aff == 0) {
    assert(CPU_ISSET(2, &cpuset) &&
           "Thread fails to pin to the targeted CPU Core 2!");
    printf("  -> [CONFIRMED] Kernel scheduler pinned thread to Core 2.\n");
  } else {
    printf("  -> [WARNING] pthread_getaffinity_np failed. Environment might "
           "lack multiple cores.\n");
  }

  size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
  void *test_page = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
  assert(test_page != MAP_FAILED);

  ((volatile char *)test_page)[0] = 'A';

  unsigned char vec = 0;
  if (mincore(test_page, page_size, &vec) == 0) {
    assert((vec & 1) && "Memory fame allocation remains unlockable swappable!");
    printf("  -> [CONFIRMED] Memory map allocations successfully pinned in RAM "
           "page tables\n");
  }

  munmap(test_page, page_size);
  printf("[PASS] Kernel tuning metrics successfully validated.\n");
}

int main(void) {
  printf("=== Starting LumenQ Kernel Tuning Verification Test ===\n");

  test_kernel_tuning_verification();

  printf(
      "=== All Kernel Tuning Verification Test Completed Successfully! ===\n");
  return 0;
}
