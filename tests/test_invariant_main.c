#include <check.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>

// Include the actual production function from main.c
extern void *allocate_buffer(size_t count, size_t size);

START_TEST(test_overflow_check_before_allocation)
{
    // Invariant: Multiplication for allocation size must not overflow
    // If overflow occurs, allocation must fail or be handled safely
    
    // Payloads: exact exploit case, boundary values, valid input
    struct {
        size_t count;
        size_t size;
        const char *description;
    } test_cases[] = {
        {SIZE_MAX, 2, "Exact exploit: multiplication wraps to small value"},
        {SIZE_MAX / 2 + 1, 2, "Boundary: just overflows when multiplied"},
        {100, 100, "Valid: normal multiplication"},
        {0, SIZE_MAX, "Boundary: zero count"},
        {SIZE_MAX, 0, "Boundary: zero size"}
    };
    
    int num_cases = sizeof(test_cases) / sizeof(test_cases[0]);
    
    for (int i = 0; i < num_cases; i++) {
        // The security property: if count * size would overflow SIZE_MAX,
        // the function must detect this and not allocate an undersized buffer
        void *result = allocate_buffer(test_cases[i].count, test_cases[i].size);
        
        // Check for overflow condition
        if (test_cases[i].count > 0 && test_cases[i].size > 0) {
            if (test_cases[i].count > SIZE_MAX / test_cases[i].size) {
                // Overflow would occur - allocation should fail or handle safely
                ck_assert_msg(result == NULL || 
                             (uintptr_t)result == (uintptr_t)0x1 || // Some error sentinel
                             false, // Force test to document the check
                             "Overflow not handled for %s", test_cases[i].description);
            } else {
                // No overflow - allocation may succeed
                // If result is non-NULL, we can't verify size, but at least we know
                // allocation was attempted with safe parameters
                if (result != NULL) {
                    free(result); // Clean up if allocation succeeded
                }
            }
        } else {
            // Zero cases - should be handled gracefully
            if (result != NULL) {
                free(result); // Clean up if allocation succeeded
            }
        }
    }
}
END_TEST

Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s = suite_create("Security");
    tc_core = tcase_create("Core");

    tcase_add_test(tc_core, test_overflow_check_before_allocation);
    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite *s;
    SRunner *sr;

    s = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}