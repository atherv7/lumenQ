#define _GNU_SOURCE

#include "lumen/consumer.h"
#include "lumen/ipc_common.h"
#include "lumen/producer.h"
#include "lumen/tuning.h"

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define TOTAL_PACKETS 10000000
#define MAX_LATENCY_NS 1000

static uint32_t latency_histogram[MAX_LATENCY_NS + 1] = {0};

void calculate_percentile(uint64_t total_samples, double cpu_ghz) {
  uint64_t accumulated = 0;

  uint64_t target_p50 = total_samples * 0.50;
  uint64_t target_p95 = total_samples * 0.95;
  uint64_t target_p99 = total_samples * 0.99;
  uint64_t target_p9999 = total_samples * 0.9999;

  double p50_ns = 0, p95_ns = 0, p99_ns = 0, p9999_ns = 0;
  uint64_t total_ns = 0;

  for (int ns = 0; ns <= MAX_LATENCY_NS; ns++) {
    uint32_t count = latency_histogram[ns];
    if (count == 0) {
      continue;
    }

    total_ns += ((uint64_t)ns * count);

    for (uint32_t c = 0; c < count; c++) {
      accumulated++;

      if (!p50_ns && accumulated >= target_p50) {
        p50_ns = ns;
      }
      if (!p95_ns && accumulated >= target_p95) {
        p95_ns = ns;
      }
      if (!p99_ns && accumulated >= target_p99) {
        p99_ns = ns;
      }
      if (!p9999_ns && accumulated >= target_p9999) {
        p9999_ns = ns;
      }
    }
  }

  double avg_ns = ((double)total_ns / (double)total_samples);

  printf("\n==================================================\n");
  printf("   LUMENQ LOCK-FREE INTER-PROCESS LATENCY SHEET    \n");
  printf("==================================================\n");
  printf(" Total Processed Packets : %lu\n", total_samples);
  printf(" CPU Speed Estimate      : %.2f GHz\n", cpu_ghz);
  printf("--------------------------------------------------\n");
  printf(" Average Latency         : %7.2f ns\n", avg_ns);
  printf(" p50 (Median)            : %7.2f ns\n", p50_ns);
  printf(" p95                     : %7.2f ns\n", p95_ns);
  printf(" p99                     : %7.2f ns\n", p99_ns);
  printf(" p99.99 (Tail Latency)   : %7.2f ns\n", p9999_ns);
  printf("==================================================\n");
}

double estimate_cpu_ghz(void) {
  uint64_t start = lumen_rdtsc();
  usleep(100000);
  uint64_t end = lumen_rdtsc();
  return (double)(end - start) / 100000000.0;
}

int main(void) {
  lumen_tune_memory_lock();
  lumen_tune_cpu_affinity(2);

  double cpu_ghz = estimate_cpu_ghz();
  printf("[BENCHMARK] Detected CPU scaling running at roughly %.2f GHz\n",
         cpu_ghz);

  LumenProducer *prod = lumen_producer_create_local();
  LumenConsumer *cons = lumen_consumer_create_local(prod);

  uint8_t dummy_data[32] = "BENCHMARK_PAYLOAD_DATA_PACKET";
  ShmFrame read_frame;
  uint64_t processed = 0;

  printf("[BENCHMARK] Streaming %d million elements...\n",
         TOTAL_PACKETS / 1000000);

  for (int i = 0; i < TOTAL_PACKETS; i++) {
    while (lumen_producer_write(prod, dummy_data, sizeof(dummy_data)) ==
           ERROR_FULL) {
      __asm__ __volatile__("pause");
    }

    while (lumen_consumer_read(cons, &read_frame) == ERROR_EMPTY) {
      __asm__ __volatile__("pause");
    }

    uint64_t end_cycle = lumen_rdtsc();
    if (end_cycle > read_frame.sender_timestamp) {
      uint64_t delta_cycles = end_cycle - read_frame.sender_timestamp;

      double delta_ns = (double)delta_cycles / cpu_ghz;
      int bucket = (int)delta_ns;

      if (bucket > MAX_LATENCY_NS) {
        bucket = MAX_LATENCY_NS;
      }
      if (bucket < 0) {
        bucket = 0;
      }

      latency_histogram[bucket]++;
      processed++;
    }
  }

  calculate_percentile(processed, cpu_ghz);

  lumen_consumer_destroy_local(cons);
  lumen_producer_destroy_local(prod);

  return 0;
}
