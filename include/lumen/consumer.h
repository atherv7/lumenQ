#ifndef LUMEN_CONSUMER_H
#define LUMEN_CONSUMER_H

#include "ipc_common.h"
#include "producer.h"
#include <stdint.h>

typedef struct LumenConsumer LumenConsumer;

LumenConsumer* lumen_consumer_create_local(LumenProducer* prod);

void lumen_consumer_destroy_local(LumenConsumer* cons);

LumenConsumer* lumen_consumer_create_ipc(const char* shm_path);

void lumen_consumer_destroy_ipc(LumenConsumer* cons);

Status lumen_consumer_read(LumenConsumer* cons, ShmFrame* out_frame);

uint32_t lumen_consumer_get_overflow_count(LumenConsumer* cons);

#endif // !LUMEN_CONSUMER_H
