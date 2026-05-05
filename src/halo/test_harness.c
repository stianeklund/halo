extern float normalize3d(float *v);

static int check(const char *name, uint32_t got, uint32_t expected, char *buf) {
    if (got == expected) {
        crt_sprintf(buf, "  PASS %s: %08X\n", name, got);
        debug_string_to_display(buf, 0);
        return 1;
    }
    crt_sprintf(buf, "  FAIL %s: got %08X expected %08X\n", name, got, expected);
    debug_string_to_display(buf, 0);
    return 0;
}

void run_tests(void) {
    char buf[128];
    int passed = 0, total = 0;

    debug_string_to_display("--- TEST_HARNESS_START ---\n", 0);

    /* normalize3d: magnitude and resulting unit vector */
    {
        float v[3] = { 10.5f, -4.2f, 3.14f };
        float mag = normalize3d(v);

        total += 4;
        passed += check("normalize3d mag", *(uint32_t*)&mag, 0x413BC96E, buf);
        passed += check("normalize3d x",   *(uint32_t*)&v[0], 0x3F650690, buf);
        passed += check("normalize3d y",   *(uint32_t*)&v[1], 0xBEB73873, buf);
        passed += check("normalize3d z",   *(uint32_t*)&v[2], 0x3E88FAA9, buf);
    }

    crt_sprintf(buf, "--- TEST_HARNESS_END: %d/%d passed ---\n", passed, total);
    debug_string_to_display(buf, 0);
}
