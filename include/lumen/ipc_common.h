#ifndef LUMEN_IPC_COMMON_H
#define LUMEN_IPC_COMMON_H

#include <stdalign.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

// ---------------------
// System configuration
// ---------------------

#ifndef CACHE_LINE_SIZE
#define CACHE_LINE_SIZE 64
#endif

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 2048
#endif

#ifndef FRAME_PAYLOAD_SIZE
#define FRAME_PAYLOAD_SIZE 52
#endif

// ---------------------
// System (static) configuration
// ---------------------

#define LUMEN_VERSION 1
#define LUMEN_MAGIC_NUMBER 0x4C554D45
#define LUMEN_DEFAULT_PATH "/lumen_shm_ring"

typedef enum {
  OK = 0,
  ERROR_FULL = -1,
  ERROR_EMPTY = -2,
  ERRPR_INVALID = -3,
  ERROR_INVALID = -4,
  ERROR_ALIGNMENT = -5
} Status;

typedef struct {
  uint64_t sender_timestamp;
  uint32_t sequence_number;
  uint8_t payload[FRAME_PAYLOAD_SIZE];
} __attribute__((aligned(CACHE_LINE_SIZE))) ShmFrame;

typedef struct {
  _Atomic uint64_t __attribute__((aligned(CACHE_LINE_SIZE))) write_ptr;
  _Atomic uint64_t __attribute__((aligned(CACHE_LINE_SIZE))) read_ptr;
  uint32_t __attribute__((aligned(CACHE_LINE_SIZE))) buffer_capacity;
  uint32_t magic_number;
  uint32_t version;
  _Atomic uint32_t overflow_count;
  _Atomic uint32_t ready;
} ShmMetadata;

typedef struct {
  ShmMetadata metadata;
  ShmFrame slots[BUFFER_SIZE];
} ShmRingBuffer;

// -----------------
// Helper functions
// -----------------

static inline uint64_t wrap_index(uint64_t absolute_index) {
  return absolute_index & (BUFFER_SIZE - 1);
}

static inline uint64_t lumen_rdtsc(void) {
  unsigned int lo, hi;

  __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
  return ((uint64_t)hi << 32) | lo;
}

// --------------------------------
// Compile-time invariant checking
// --------------------------------

// Ensure the buffer size is strictly a power of 2
_Static_assert((BUFFER_SIZE & (BUFFER_SIZE - 1)) == 0,
               "ERROR: BUFFER_SIZE must be a power of 2");

// Ensure the control structuress are properly isolated to prevent overlapping
// cache lines
_Static_assert(
    offsetof(ShmMetadata, read_ptr) >= CACHE_LINE_SIZE,
    "ERROR: False sharing danger, read_ptr is too close to write_ptr");

// Ensure data frame fills an exact multiple of a cache line
_Static_assert((sizeof(ShmFrame) % CACHE_LINE_SIZE) == 0,
               "ERROR: ShmFrame size must be perfectly divisible by 64 bytes "
               "to align with cache slots");

#endif // !LUMEN_IPC_COMMON_H
