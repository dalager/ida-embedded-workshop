// Host unit tests for the chunked-upload part-plan math (net/upload_plan.c). Pure C, no ESP-IDF.
// Firmware streams multipart parts. S3-style object stores' CompleteMultipartUpload requires every
// NON-last part to be exactly part_size; the last part is the remainder. These tests pin that rule.
#include "unity.h"
#include "upload_plan.h"

#define PART_SIZE (8u * 1024u * 1024u)  // 8 MiB = 8388608

void setUp(void) {}
void tearDown(void) {}

// Small helper: assert a plan equals the expected {n_parts, last_part_size}.
static void assert_plan(uint64_t file_size, uint32_t part_size,
                        uint32_t exp_n, uint64_t exp_last) {
    upload_plan_t p = upload_plan(file_size, part_size);
    TEST_ASSERT_EQUAL_UINT32(exp_n, p.n_parts);
    TEST_ASSERT_EQUAL_UINT64(exp_last, p.last_part_size);
}

// ── upload_plan: the coder's smoke cases as real assertions ──────────────────
void test_empty_file(void) {
    // 0 bytes -> no parts at all.
    assert_plan(0, PART_SIZE, 0, 0);
}

void test_single_byte(void) {
    assert_plan(1, PART_SIZE, 1, 1);
}

void test_one_less_than_part(void) {
    // Just under one full part -> single part holding the whole remainder.
    assert_plan(PART_SIZE - 1, PART_SIZE, 1, PART_SIZE - 1);
}

void test_exactly_one_part(void) {
    // Exactly part_size -> one part, last == part_size.
    assert_plan(PART_SIZE, PART_SIZE, 1, PART_SIZE);
}

void test_one_more_than_part(void) {
    // Crossing the boundary -> a second part carrying the 1-byte remainder.
    assert_plan((uint64_t)PART_SIZE + 1, PART_SIZE, 2, 1);
}

void test_exact_multiple_three_parts(void) {
    // Exact multiple -> N full parts, last part == part_size (NOT zero).
    assert_plan((uint64_t)PART_SIZE * 3, PART_SIZE, 3, PART_SIZE);
}

void test_typical_15mb_recording(void) {
    // 15,000,000 bytes -> 2 parts; last = 15000000 - 8388608 = 6611392.
    assert_plan(15000000ull, PART_SIZE, 2, 15000000ull - PART_SIZE);
}

// ── defensive / edge ─────────────────────────────────────────────────────────
void test_zero_part_size_defensive(void) {
    // A 0 part_size must not divide-by-zero; yields an empty plan.
    assert_plan(15000000ull, 0, 0, 0);
}

// ── part_offset ──────────────────────────────────────────────────────────────
void test_part_offset_basic(void) {
    TEST_ASSERT_EQUAL_UINT64(0, part_offset(1, PART_SIZE));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)PART_SIZE, part_offset(2, PART_SIZE));
    TEST_ASSERT_EQUAL_UINT64((uint64_t)PART_SIZE * 2, part_offset(3, PART_SIZE));
}

void test_part_offset_zero_partnum(void) {
    // Out-of-range 0 is treated as offset 0.
    TEST_ASSERT_EQUAL_UINT64(0, part_offset(0, PART_SIZE));
}

// ── part_len: per-part length lookup + out-of-range ──────────────────────────
void test_part_len_out_of_range(void) {
    upload_plan_t p = upload_plan((uint64_t)PART_SIZE * 3, PART_SIZE);  // {3, PART_SIZE}
    TEST_ASSERT_EQUAL_UINT64(0, part_len(0, &p, PART_SIZE));            // pn == 0
    TEST_ASSERT_EQUAL_UINT64(0, part_len(4, &p, PART_SIZE));            // pn > n_parts
    TEST_ASSERT_EQUAL_UINT64(0, part_len(1, NULL, PART_SIZE));          // null plan
}

void test_part_len_last_is_remainder(void) {
    // Non-multiple: first parts full, last is the remainder.
    upload_plan_t p = upload_plan(15000000ull, PART_SIZE);  // {2, 6611392}
    TEST_ASSERT_EQUAL_UINT64((uint64_t)PART_SIZE, part_len(1, &p, PART_SIZE));
    TEST_ASSERT_EQUAL_UINT64(15000000ull - PART_SIZE, part_len(2, &p, PART_SIZE));
}

// ── plan-wide invariants: every non-last == part_size, offsets monotonic ──────
static void check_invariants(uint64_t file_size, uint32_t part_size) {
    upload_plan_t p = upload_plan(file_size, part_size);
    if (p.n_parts == 0) { return; }

    uint64_t sum = 0;
    uint64_t prev_off = 0;
    for (uint32_t pn = 1; pn <= p.n_parts; pn++) {
        uint64_t len = part_len(pn, &p, part_size);
        uint64_t off = part_offset(pn, part_size);

        if (pn < p.n_parts) {
            // Every NON-last part is exactly part_size (S3-style rule).
            TEST_ASSERT_EQUAL_UINT64((uint64_t)part_size, len);
        } else {
            // Last part is the recorded remainder, and it is 1..part_size (never 0).
            TEST_ASSERT_EQUAL_UINT64(p.last_part_size, len);
            TEST_ASSERT_TRUE(len >= 1 && len <= (uint64_t)part_size);
        }

        // Offset for part pn must equal bytes already summed, and be strictly monotonic.
        TEST_ASSERT_EQUAL_UINT64(sum, off);
        if (pn > 1) { TEST_ASSERT_TRUE(off > prev_off); }
        prev_off = off;
        sum += len;
    }
    // The parts must exactly reconstruct the file.
    TEST_ASSERT_EQUAL_UINT64(file_size, sum);
}

void test_invariants_across_sizes(void) {
    check_invariants(1, PART_SIZE);
    check_invariants(PART_SIZE - 1, PART_SIZE);
    check_invariants(PART_SIZE, PART_SIZE);
    check_invariants((uint64_t)PART_SIZE + 1, PART_SIZE);
    check_invariants((uint64_t)PART_SIZE * 3, PART_SIZE);
    check_invariants(15000000ull, PART_SIZE);
    check_invariants((uint64_t)PART_SIZE * 5 + 12345, PART_SIZE);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_file);
    RUN_TEST(test_single_byte);
    RUN_TEST(test_one_less_than_part);
    RUN_TEST(test_exactly_one_part);
    RUN_TEST(test_one_more_than_part);
    RUN_TEST(test_exact_multiple_three_parts);
    RUN_TEST(test_typical_15mb_recording);
    RUN_TEST(test_zero_part_size_defensive);
    RUN_TEST(test_part_offset_basic);
    RUN_TEST(test_part_offset_zero_partnum);
    RUN_TEST(test_part_len_out_of_range);
    RUN_TEST(test_part_len_last_is_remainder);
    RUN_TEST(test_invariants_across_sizes);
    return UNITY_END();
}
