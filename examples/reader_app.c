#define _DEFAULT_SOURCE

#include "lumen/consumer.h"
#include "lumen/ipc_common.h"
#include "lumen/tuning.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
  printf("[Consumer Engine] Initializing real-time OS optimization...\n");

  lumen_tune_memory_lock();
  lumen_tune_cpu_affinity(3);

  printf("[Consumer Engine] Connecting to shared ring buffer channel...\n");

  LumenConsumer* cons = NULL;
  while ((cons = lumen_consumer_create_ipc(LUMEN_DEFAULT_PATH)) == NULL) {
    printf("Waiting for shared memory segment configuration files...\n");
    sleep(1);
  }

  printf("[Consumer Engine] Attached successfully. Core affinity bound to core "
         "3.\n");
  printf("[Consumer Engine] Polling loop active...\n");

  ShmFrame frame;
  uint32_t last_overflow_count = 0;

  last_overflow_count = lumen_consumer_get_overflow_count(cons);
  if (last_overflow_count > 0) {
    printf("[INIT] System recovered with %u historically dropped frames.\n",
           last_overflow_count);
  }

  while (1) {
    Status stat = lumen_consumer_read(cons, &frame);
    if (stat == OK) {
      printf("Received Sequence [%u]: %s\n", frame.sequence_number,
             (char*)frame.payload);
    } else if (stat == ERROR_EMPTY) {
      usleep(10000);
    }

    uint32_t current_overflows = lumen_consumer_get_overflow_count(cons);
    if (current_overflows > last_overflow_count) {
      uint32_t delta = current_overflows - last_overflow_count;
      fprintf(stderr,
              "[WARNING] Slow Consumer Alert! Producer dropped %u frame(s). "
              "Total dropped packets: %u\n",
              delta, current_overflows);

      last_overflow_count = current_overflows;
    }
  }

  lumen_consumer_destroy_ipc(cons);
  return 0;
}
