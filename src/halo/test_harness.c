extern float normalize3d(float *v);

void run_tests(void) {
    char buf[128];

    debug_string_to_display("--- TEST_HARNESS_START ---\n", 0);

    float test_vec[3] = { 10.5f, -4.2f, 3.14f };
    float result = normalize3d(test_vec);

    crt_sprintf(buf, "Result Mag: %08X\n", *(uint32_t*)&result);
    debug_string_to_display(buf, 0);
    crt_sprintf(buf, "Vec X: %08X\n", *(uint32_t*)&test_vec[0]);
    debug_string_to_display(buf, 0);
    crt_sprintf(buf, "Vec Y: %08X\n", *(uint32_t*)&test_vec[1]);
    debug_string_to_display(buf, 0);
    crt_sprintf(buf, "Vec Z: %08X\n", *(uint32_t*)&test_vec[2]);
    debug_string_to_display(buf, 0);

    debug_string_to_display("--- TEST_HARNESS_END ---\n", 0);
}
