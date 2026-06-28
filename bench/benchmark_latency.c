#include <stdint.h>
#define _GNU_SOURCE

#include "lumen/consumer.h"
#include "lumen/ipc_common.h"
#include "lumen/producer.h"
#include "lumen/tuning.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TOTAL_PACKETS 10000000
#define MAX_LATENCY_US 1000

static unint32_t latency_histogram[MAX_LATENCY_US + 1] = {0};

void calculate_percentile(uint64_t total_samples, double cpu_ghz) {
  uint64_t accumulated = 0;
}
