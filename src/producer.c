#include "lumen/producer.h"
#include "lumen/ipc_common.h"
#include "lumen/shm_utils.h"

#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct LumenProducer {
  ShmRingBuffer *ring_buffer;
  int is_local;
  int shm_fd;
};

LumenProducer *lumen_producer_create_local(void) {
  LumenProducer *prod = malloc(sizeof(LumenProducer));
  if (!prod) {
    return NULL;
  }

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
  prod->ring_buffer = aligned_alloc(CACHE_LINE_SIZE, sizeof(ShmRingBuffer));
#else
  prod->ring_buffer = malloc(sizeof(ShmRingBuffer));
#endif

  if (!prod->ring_buffer) {
    free(prod);
    return NULL;
  }

  memset(prod->ring_buffer, 0, sizeof(ShmRingBuffer));
  prod->ring_buffer->metadata.magic_number = LUMEN_MAGIC_NUMBER;
  prod->ring_buffer->metadata.version = LUMEN_VERSION;
  prod->ring_buffer->metadata.buffer_capacity = BUFFER_SIZE;

  atomic_init(&prod->ring_buffer->metadata.write_ptr, 0);
  atomic_init(&prod->ring_buffer->metadata.read_ptr, 0);
  atomic_init(&prod->ring_buffer->metadata.overflow_count, 0);

  prod->is_local = 1;
  return prod;
}

LumenProducer *lumen_producer_create_ipc(const char *shm_path) {
  LumenProducer *prod = malloc(sizeof(LumenProducer));
  if (!prod) {
    return NULL;
  }

  prod->shm_fd = -1;
  prod->is_local = 0;

  prod->ring_buffer = (ShmRingBuffer *)lumen_shm_map(
      shm_path, sizeof(ShmRingBuffer), 1, &prod->shm_fd);
  if (!prod->ring_buffer) {
    free(prod);
    return NULL;
  }

  memset(prod->ring_buffer, 0, sizeof(ShmRingBuffer));
  prod->ring_buffer->metadata.magic_number = LUMEN_MAGIC_NUMBER;
  prod->ring_buffer->metadata.version = LUMEN_VERSION;
  prod->ring_buffer->metadata.buffer_capacity = BUFFER_SIZE;

  atomic_init(&prod->ring_buffer->metadata.write_ptr, 0);
  atomic_init(&prod->ring_buffer->metadata.read_ptr, 0);
  atomic_init(&prod->ring_buffer->metadata.overflow_count, 0);

  return prod;
}

void lumen_producer_destroy_local(LumenProducer *prod) {
  if (prod) {
    if (prod->is_local && prod->ring_buffer) {
      free(prod->ring_buffer);
    }
    free(prod);
  }
}

void lumen_producer_destroy_ipc(LumenProducer *prod, const char *shm_path) {
  if (prod) {
    if (!prod->is_local && prod->ring_buffer) {
      lumen_shm_unmap(prod->ring_buffer, sizeof(ShmRingBuffer), prod->shm_fd,
                      shm_path, 1);
    }

    free(prod);
  }
}

Status lumen_producer_write(LumenProducer *prod, const uint8_t *data,
                            uint32_t len) {
  if (!prod || !prod->ring_buffer || !data || len > FRAME_PAYLOAD_SIZE) {
    return ERROR_INVALID;
  }

  ShmRingBuffer *buf = prod->ring_buffer;

  uint64_t current_write =
      atomic_load_explicit(&buf->metadata.write_ptr, memory_order_relaxed);
  uint64_t current_read =
      atomic_load_explicit(&buf->metadata.read_ptr, memory_order_acquire);

  if (current_write - current_read >= buf->metadata.buffer_capacity) {
    atomic_fetch_add_explicit(&buf->metadata.overflow_count, 1,
                              memory_order_relaxed);

    return ERROR_FULL;
  }

  uint32_t index = current_write % buf->metadata.buffer_capacity;
  ShmFrame *slot = &buf->slots[index];

  memset(slot->payload, 0, sizeof(slot->payload));
  memcpy(slot->payload, data, len);
  slot->sequence_number = (uint32_t)current_write;
  slot->sender_timestamp = lumen_rdtsc();

  atomic_store_explicit(&buf->metadata.write_ptr, current_write + 1,
                        memory_order_release);
  return OK;
}

ShmRingBuffer *idx_internal_get_buf(LumenProducer *prod) {
  return prod ? prod->ring_buffer : NULL;
}
