#include <stdio.h>
#include <stdint.h>

extern float normalize3d(float *v);

void run_tests(void) {
    printf("\n--- TEST_HARNESS_START ---\n");

    // Use a volatile/stack vector to prevent the compiler from optimizing it away
    float test_vec[3] = { 10.5f, -4.2f, 3.14f };
    
    // Run the function
    float result = normalize3d(test_vec);

    // ALWAYS print floats as hex (uint32_t) to catch microscopic precision/rounding errors
    printf("Result Mag: %08X\n", *(uint32_t*)&result);
    printf("Vec X: %08X\n", *(uint32_t*)&test_vec[0]);
    printf("Vec Y: %08X\n", *(uint32_t*)&test_vec[1]);
    printf("Vec Z: %08X\n", *(uint32_t*)&test_vec[2]);

    printf("\n--- TEST_HARNESS_END ---\n");
}