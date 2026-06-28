#define _GNU_SOURCE
#include "lumen/tuning.h"
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <sys/mman.h>

void lumen_tune_memory_lock(void) {
  if (mlockall(MCL_CURRENT | MCL_FUTURE) == -1) {
    perror("[TUNING WARNING] mlockall failed (Run as root/sudo?)");
  }
}

void lumen_tune_cpu_affinity(int core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
}
