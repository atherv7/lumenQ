#define _DEFAULT_SOURCE

#include "lumen/consumer.h"
#include "lumen/ipc_common.h"

#include <stdio.h>
#include <unistd.h>

int main(void) {
  printf("[Consumer Engine] Connecting to shared ring buffer channel...\n");

  LumenConsumer *cons = NULL;
  while ((cons = lumen_consumer_create_ipc(LUMEN_DEFAULT_PATH)) == NULL) {
    printf("Waiting for shared memory segment configuration files...\n");
    sleep(1);
  }

  printf("[Consumer Engine] Attached successfully. Polling loop active\n");
  ShmFrame frame;

  while (1) {
    Status stat = lumen_consumer_read(cons, &frame);
    if (stat == OK) {
      printf("Received Sequence [%u]: %s\n", frame.sequence_number,
             (char *)frame.payload);
    } else if (stat == ERROR_EMPTY) {
      usleep(10000);
    }
  }

  lumen_consumer_destroy_ipc(cons);
  return 0;
}
