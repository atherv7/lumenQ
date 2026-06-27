#include "lumen/consumer.h"
#include "lumen/ipc_common.h"
#include "lumen/producer.h"

#include <assert.h>
#include <bits/pthreadtypes.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_COUNT 500000

typedef struct {
  LumenProducer *prod;
  LumenConsumer *cons;
} ThreadContext;

static void test_basic_bounds(void) {
  printf("[TEST] Running basic functional and boundary checks...");

  LumenProducer *prod = lumen_producer_create_local();
  LumenConsumer *cons = lumen_consumer_create_local(prod);

  ShmFrame out_frame;

  assert(lumen_consumer_read(cons, &out_frame) == ERROR_EMPTY);

  uint8_t mock_data[10] = "LumenQ";
  assert(lumen_producer_write(prod, mock_data, 10) == OK);
  assert(lumen_consumer_read(cons, &out_frame) == OK);
  assert(out_frame.sequence_number == 0);
  assert(memcmp(out_frame.payload, "LumenQ", 10) == 0);

  uint8_t dummy[4] = {0};
  for (int i = 0; i < BUFFER_SIZE; i++) {
    assert(lumen_producer_write(prod, dummy, sizeof(dummy)) == OK);
  }

  assert(lumen_producer_write(prod, dummy, sizeof(dummy)) == ERROR_FULL);

  lumen_consumer_destroy_local(cons);
  lumen_producer_destroy_local(prod);

  printf("[PASS] Basic functional boundaries verified.\n");
}

void *producer_thread_worker(void *arg) {
  LumenProducer *prod = ((ThreadContext *)arg)->prod;
  uint32_t payload_val = 0xAABBCCDD;

  for (uint32_t i = 0; i < TEST_COUNT; i++) {
    uint8_t data[4];
    memcpy(data, &payload_val, 4);

    while (lumen_producer_write(prod, data, 4) == ERROR_FULL) {
      // busy wait
    }
  }

  return NULL;
}

void *consumer_thread_worker(void *arg) {
  LumenConsumer *cons = ((ThreadContext *)arg)->cons;
  ShmFrame received_frame;
  uint32_t expected_seq = 0;

  while (expected_seq < TEST_COUNT) {
    Status stat = lumen_consumer_read(cons, &received_frame);

    if (stat == OK) {
      if (received_frame.sequence_number != expected_seq) {
        fprintf(
            stderr,
            "[FAIL] Memory Out-of-Order detected! Expected seq %u, got %u\n",
            expected_seq, received_frame.sequence_number);
        exit(EXIT_FAILURE);
      }

      uint32_t payload_check;
      memcpy(&payload_check, received_frame.payload, 4);
      assert(payload_check == 0xAABBCCDD);

      expected_seq++;
    } else if (stat == ERROR_EMPTY) {
      continue;
    } else {
      fprintf(stderr,
              "[FAIL] Consumer encountered fatal structural error status: %d\n",
              stat);
      exit(EXIT_FAILURE);
    }
  }

  return NULL;
}

static void test_concurrent_race_conditions(void) {
  printf("[TEST] Launching multi-threaded pipeline stress check (%d "
         "iterations)...",
         TEST_COUNT);

  LumenProducer *prod = lumen_producer_create_local();
  LumenConsumer *cons = lumen_consumer_create_local(prod);

  ThreadContext ctx = {.prod = prod, .cons = cons};
  pthread_t prod_tid, cons_tid;

  assert(pthread_create(&prod_tid, NULL, producer_thread_worker, &ctx) == 0);
  assert(pthread_create(&cons_tid, NULL, consumer_thread_worker, &ctx) == 0);

  pthread_join(prod_tid, NULL);
  pthread_join(cons_tid, NULL);

  lumen_consumer_destroy_local(cons);
  lumen_producer_destroy_local(prod);

  printf("[PASS] High-throughput race test passed with zero data loss or "
         "corruption");
}

int main(void) {
  printf("=== Starting LumenQ Verification Test ===");

  test_basic_bounds();
  test_concurrent_race_conditions();

  printf("=== All Verification Tests Completed Successfully! === \n");
  return 0;
}
