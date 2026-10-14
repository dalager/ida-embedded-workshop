#pragma once
#include <stdint.h>

// Pure part-plan math for a chunked / multipart upload. No ESP-IDF dependency so it is
// host-unit-testable (test/host/test_upload_plan.c).
//
// S3-style multipart rule (enforced at CompleteMultipartUpload): every NON-last part must be exactly
// `part_size`; the last part is the remainder (no minimum). This module is the single source of
// truth for how a file of `file_size` bytes splits into N equal-then-remainder parts.

typedef struct {
    uint32_t n_parts;         // number of parts (0 when file_size == 0)
    uint64_t last_part_size;  // byte length of the final part (== part_size on an exact multiple)
} upload_plan_t;

// Compute the part plan for `file_size` bytes at `part_size` per part.
//   file_size == 0            -> { 0, 0 }
//   file_size <= part_size    -> { 1, file_size }
//   exact multiple            -> { file_size/part_size, part_size }
// `part_size` must be > 0 (a 0 part_size yields { 0, 0 } defensively).
upload_plan_t upload_plan(uint64_t file_size, uint32_t part_size);

// Byte offset of part `part_number` (1-based) within the file: (part_number - 1) * part_size.
uint64_t part_offset(uint32_t part_number, uint32_t part_size);

// Byte length of part `part_number` (1-based) given a computed plan: part_size for every part
// except the last, which is plan.last_part_size. Returns 0 for an out-of-range part_number.
uint64_t part_len(uint32_t part_number, const upload_plan_t *plan, uint32_t part_size);
