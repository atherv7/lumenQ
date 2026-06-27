#include "lumen/consumer.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

ShmRingBuffer *idx_internal_get_buf(LumenProducer *prod);

struct LumenConsumer {
  ShmRingBuffer *ring_buffer;
};

LumenConsumer *lumen_consumer_create_local(LumenProducer *prod) {
  if (!prod) {
    return NULL;
  }

  LumenConsumer *cons = malloc(sizeof(LumenConsumer));
  if (!cons) {
    return NULL;
  }

  cons->ring_buffer = idx_internal_get_buf(prod);
  if (!cons->ring_buffer) {
    free(cons);
    return NULL;
  }

  return cons;
}

void lumen_consumer_destroy_local(LumenConsumer *cons) {
  if (cons) {
    free(cons);
  }
}

Status lumen_consumer_read(LumenConsumer *cons, ShmFrame *out_frame) {
  if (!cons || !cons->ring_buffer || !out_frame) {
    return ERROR_INVALID;
  }

  ShmMetadata *meta = &cons->ring_buffer->metadata;

  uint64_t current_read =
      atomic_load_explicit(&meta->read_ptr, memory_order_relaxed);

  uint64_t current_write =
      atomic_load_explicit(&meta->write_ptr, memory_order_acquire);

  if (current_read == current_write) {
    return ERROR_EMPTY;
  }

  uint64_t index = wrap_index(current_read);
  ShmFrame *slot = &cons->ring_buffer->slots[index];

  memcpy(out_frame, slot, sizeof(ShmFrame));

  atomic_store_explicit(&meta->read_ptr, current_read + 1,
                        memory_order_relaxed);

  return OK;
}
