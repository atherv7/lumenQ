#define _DEFAULT_SOURCE

#include "lumen/consumer.h"
#include "lumen/ipc_common.h"
#include "lumen/producer.h"

#include <assert.h>
#include <bits/pthreadtypes.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define TEST_COUNT 500000

typedef struct {
  LumenProducer* prod;
  LumenConsumer* cons;
} ThreadContext;

static void test_basic_bounds(void) {
  printf("[TEST] Running basic functional and boundary checks...");

  LumenProducer* prod = lumen_producer_create_local();
  LumenConsumer* cons = lumen_consumer_create_local(prod);

  ShmFrame out_frame;

  assert(lumen_consumer_read(cons, &out_frame) == ERROR_EMPTY);

  uint8_t mock_data[10] = "LumenQ";
  assert(lumen_producer_write(prod, mock_data, 10) == OK);
  assert(lumen_consumer_read(cons, &out_frame) == OK);
  assert(out_frame.sequence_number == 0);
  assert(memcmp(out_frame.payload, mock_data, 10) == 0);

  uint8_t dummy[4] = {0};
  for (int i = 0; i < BUFFER_SIZE; i++) {
    assert(lumen_producer_write(prod, dummy, sizeof(dummy)) == OK);
  }

  assert(lumen_producer_write(prod, dummy, sizeof(dummy)) == ERROR_FULL);

  lumen_consumer_destroy_local(cons);
  lumen_producer_destroy_local(prod);

  printf("[PASS] Basic functional boundaries verified.\n");
}

void* producer_thread_worker(void* arg) {
  LumenProducer* prod = ((ThreadContext*)arg)->prod;
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

void* consumer_thread_worker(void* arg) {
  LumenConsumer* cons = ((ThreadContext*)arg)->cons;
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

  LumenProducer* prod = lumen_producer_create_local();
  LumenConsumer* cons = lumen_consumer_create_local(prod);

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

static void test_ipc_cross_process(const char* shm_path) {
  printf("[TEST] Verifying cross-process IPC boundary validation...\n");

  pid_t pid = fork();
  assert(pid != -1);

  if (pid == 0) {
    LumenProducer* prod = lumen_producer_create_ipc(shm_path);
    if (!prod) {
      exit(EXIT_FAILURE);
    }

    uint8_t payload[10] = "IPC_DATA";

    usleep(50000);

    if (lumen_producer_write(prod, payload, 10) != OK) {
      exit(EXIT_FAILURE);
    }

    lumen_producer_destroy_ipc(prod, shm_path);
    exit(EXIT_SUCCESS);
  } else {
    LumenConsumer* cons = NULL;

    for (int i = 0; i < 100; i++) {
      cons = lumen_consumer_create_ipc(shm_path);
      if (cons) {
        break;
      }
      usleep(50000);
    }
    assert(cons != NULL);

    ShmFrame out_frame;
    Status stat = ERROR_EMPTY;

    for (int i = 0; i < 1000 && stat == ERROR_EMPTY; i++) {
      stat = lumen_consumer_read(cons, &out_frame);
      if (stat == ERROR_EMPTY) {
        usleep(1000);
      }
    }

    uint8_t payload[10] = "IPC_DATA";

    assert(stat == OK);
    assert(memcmp(out_frame.payload, payload, 10) == 0);

    int status;
    waitpid(pid, &status, 0);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == EXIT_SUCCESS);

    lumen_consumer_destroy_ipc(cons);
    printf("[PASS] Cross-process memory pipeline verified successfully.\n");
  }
}

static void test_ipc_consumer_starts_first(const char* shm_path) {
  printf(
      "[TEST] Verifying consumer resilience when starting before prodcuer...");

  shm_unlink(shm_path);

  LumenConsumer* cons = lumen_consumer_create_ipc(shm_path);
  assert(cons == NULL);

  LumenProducer* prod = lumen_producer_create_ipc(shm_path);
  assert(prod != NULL);

  cons = lumen_consumer_create_ipc(shm_path);
  assert(cons != NULL);

  lumen_consumer_destroy_ipc(cons);
  lumen_producer_destroy_ipc(prod, shm_path);
  printf("[PASS] Consumer gracefully handled uninitialized shared memory.\n");
}

static void test_ipc_corrupt_metadata_validation(const char* shm_path) {
  printf("[TEST] Verifying validation of corrupted or malformed memory "
         "segments...");

  int fd = shm_open(shm_path, O_CREAT | O_RDWR | O_TRUNC, 0666);
  assert(fd != -1);
  assert(ftruncate(fd, sizeof(ShmRingBuffer)) == 0);

  void* raw_ptr = mmap(NULL, sizeof(ShmRingBuffer), PROT_READ | PROT_WRITE,
                       MAP_SHARED, fd, 0);
  assert(raw_ptr != MAP_FAILED);

  ShmRingBuffer* bad_buf = (ShmRingBuffer*)raw_ptr;
  bad_buf->metadata.magic_number = 0xDEADBEEF;
  bad_buf->metadata.buffer_capacity = 999999;

  munmap(raw_ptr, sizeof(ShmRingBuffer));
  close(fd);

  LumenConsumer* cons = lumen_consumer_create_ipc(shm_path);
  assert(cons == NULL);

  shm_unlink(shm_path);
  printf(
      "[PASS] Corrupt layout validation successfully rejected bad segment.\n");
}

static void test_ipc_buffer_saturation(const char* shm_path) {
  printf("[TEST] Verifying overflow diagnostics under process saturation...\n");

  LumenProducer* prod = lumen_producer_create_ipc(shm_path);
  assert(prod != NULL);

  uint8_t dummy[10] = "PACKET";

  int writes = 0;
  int dropped = 0;

  for (int i = 0; i < BUFFER_SIZE + 5; i++) {
    Status stat = lumen_producer_write(prod, dummy, sizeof(dummy));
    if (stat == OK) {
      writes++;
    }
    if (stat == ERROR_FULL) {
      dropped++;
    }
  }

  assert(writes == BUFFER_SIZE);
  assert(dropped == 5);

  LumenConsumer* cons = lumen_consumer_create_ipc(shm_path);
  assert(cons != NULL);

  lumen_consumer_destroy_ipc(cons);
  lumen_producer_destroy_ipc(prod, shm_path);
  printf("[PASS] Overrun diagnostic tracking fully verified.\n");
}

int main(void) {
  printf("=== Starting LumenQ Race Condition Verification Test ===\n");

  test_basic_bounds();
  test_concurrent_race_conditions();

  const char* test_shm_path = "/lumenq_test_buffer";

  test_ipc_consumer_starts_first(test_shm_path);
  test_ipc_corrupt_metadata_validation(test_shm_path);
  test_ipc_buffer_saturation(test_shm_path);
  test_ipc_cross_process(test_shm_path);

  printf("=== All Race Condition Verification Tests Completed Successfully! "
         "=== \n");
  return 0;
}
