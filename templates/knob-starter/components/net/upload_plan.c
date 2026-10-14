#include "upload_plan.h"
#include <stdbool.h>

upload_plan_t upload_plan(uint64_t file_size, uint32_t part_size)
{
    upload_plan_t p = { 0, 0 };
    if (file_size == 0 || part_size == 0) return p;

    // ceil(file_size / part_size) without overflow: parts fits in uint32 for any real session
    // (15 MB / 8 MiB = 2; even 5 GiB / 5 MiB = 1024, far below the 10,000-part ceiling of S3-style stores).
    uint64_t n = (file_size + (uint64_t)part_size - 1) / (uint64_t)part_size;
    p.n_parts = (uint32_t)n;

    uint64_t whole = (n - 1) * (uint64_t)part_size;   // bytes covered by the non-last parts
    p.last_part_size = file_size - whole;             // remainder; == part_size on exact multiple
    return p;
}

uint64_t part_offset(uint32_t part_number, uint32_t part_size)
{
    if (part_number == 0) return 0;
    return (uint64_t)(part_number - 1) * (uint64_t)part_size;
}

static bool part_in_plan(uint32_t part_number, const upload_plan_t *plan)
{
    return plan && part_number >= 1 && part_number <= plan->n_parts;
}

uint64_t part_len(uint32_t part_number, const upload_plan_t *plan, uint32_t part_size)
{
    if (!part_in_plan(part_number, plan)) return 0;
    return (part_number == plan->n_parts) ? plan->last_part_size : (uint64_t)part_size;
}
