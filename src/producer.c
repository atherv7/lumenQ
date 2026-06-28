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
  if (!prod || !prod->ring_buffer || !data) {
    return ERROR_INVALID;
  }
  if (len > sizeof(((ShmFrame *)0)->payload)) {
    return ERROR_INVALID;
  }

  ShmMetadata *meta = &prod->ring_buffer->metadata;

  uint64_t current_write =
      atomic_load_explicit(&meta->write_ptr, memory_order_relaxed);
  uint64_t current_read =
      atomic_load_explicit(&meta->read_ptr, memory_order_relaxed);

  if (current_write - current_read >= BUFFER_SIZE) {
    atomic_fetch_add_explicit(&meta->overflow_count, 1, memory_order_relaxed);
    return ERROR_FULL;
  }

  uint64_t index = wrap_index(current_write);
  ShmFrame *slot = &prod->ring_buffer->slots[index];

  memcpy(slot->payload, data, len);
  slot->sequence_number = (uint32_t)current_write;

  slot->sender_timestamp = 0;

  atomic_store_explicit(&meta->write_ptr, current_write + 1,
                        memory_order_release);

  return OK;
}

ShmRingBuffer *idx_internal_get_buf(LumenProducer *prod) {
  return prod ? prod->ring_buffer : NULL;
}
