#include "lumen/consumer.h"
#include "lumen/ipc_common.h"
#include "lumen/producer.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void test_drop_and_log(void) {
  printf("[TEST] Verifying 'Drop & Log' saturation invariant...\n");

  LumenProducer *prod = lumen_producer_create_local();
  assert(prod != NULL);

  uint8_t payload[] = "TELEMETRY";
  int success_writes = 0;
  int dropped_writes = 0;
  int target_overflows = 5;

  for (int i = 0; i < BUFFER_SIZE + target_overflows; i++) {
    Status stat = lumen_producer_write(prod, payload, sizeof(payload));
    if (stat == OK) {
      success_writes++;
    } else if (stat == ERROR_FULL) {
      dropped_writes++;
    }
  }

  assert(success_writes == BUFFER_SIZE &&
         "Producer over-allocated past internal ring capacity constraints!");
  assert(dropped_writes == target_overflows &&
         "Producer blocked or failed to drop packet cleanly upon buffer "
         "saturation!");

  struct HiddenProducer {
    ShmRingBuffer *ring_buffer;
  };
  ShmRingBuffer *buf = ((struct HiddenProducer *)prod)->ring_buffer;

  uint32_t tracked_overflows = atomic_load(&buf->metadata.overflow_count);
  assert(tracked_overflows == (uint32_t)target_overflows &&
         "Atomic overflow tracking failed o record drops accurately");

  lumen_producer_destroy_local(prod);
  printf("[PASS] Drop & Log metrics verified. Overflows tracked cleanly: %u\n",
         tracked_overflows);
}

int main(void) {
  printf("=== Starting LumenQ 'Drop & Log' Verification Test ===\n");

  test_drop_and_log();

  printf(
      "=== All 'Drop & Log' Verification Test Completed Successfully! === \n");

  return 0;
}
