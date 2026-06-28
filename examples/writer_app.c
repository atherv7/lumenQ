#define _DEFAULT_SOURCE

#include "lumen/ipc_common.h"
#include "lumen/producer.h"
#include "lumen/tuning.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int main(void) {
  printf("[Producer Engine] Initializing real-time OS optimization...\n");

  lumen_tune_memory_lock();
  lumen_tune_cpu_affinity(2);

  printf("[Producer Engine] Initializing shared ring buffer...\n");
  LumenProducer *prod = lumen_producer_create_ipc(LUMEN_DEFAULT_PATH);
  if (!prod) {
    return 1;
  }

  uint32_t message_counter = 0;
  printf("[Producer Engine] Streaming events, Press Ctrl+C to terminate.\n");

  while (1) {
    char msg_buffer[32];
    snprintf(msg_buffer, sizeof(msg_buffer), "MARKET_TICK_%u", message_counter);

    Status stat =
        lumen_producer_write(prod, (uint8_t *)msg_buffer, sizeof(msg_buffer));
    if (stat == OK) {
      printf("Sent: %s\n", msg_buffer);
      message_counter++;
    } else if (stat == ERROR_FULL) {
      fprintf(stderr, "[DROP ALERT] Ring buffer saturated! Dropped event count "
                      "tracking incremented.\n");
    }

    usleep(500000);
  }

  lumen_producer_destroy_ipc(prod, LUMEN_DEFAULT_PATH);
  return 0;
}
