#ifndef LUMEN_PRODUCER_H
#define LUMEN_PRODUCER_H

#include "ipc_common.h"

typedef struct LumenProducer LumenProducer;

LumenProducer *lumen_producer_create_local(void);

void lumen_producer_destroy_local(LumenProducer *prod);

LumenProducer *lumen_producer_create_ipc(const char *shm_path);

void lumen_producer_destroy_ipc(LumenProducer *prod, const char *shm_path);

Status lumen_producer_write(LumenProducer *prod, const uint8_t *data,
                            uint32_t len);

#endif // !LUMEN_PRODUCER_H
