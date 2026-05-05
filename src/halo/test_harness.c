extern float normalize3d(float *v);
extern void vector3d_scale_add(float *base, float *direction, float scale, float *out);
extern void matrix_transform_point(float *matrix, float *in, float *out);
extern void perpendicular3d(float *in, float *out);
extern void angles_to_vector(float *out, float *angles);

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

    /* vector3d_scale_add */
    {
        float base[3] = { 1.5f, 2.0f, -3.2f };
        float dir[3]  = { 0.5f, -1.0f, 2.0f };
        float out[3];
        
        vector3d_scale_add(base, dir, 2.5f, out);
        
        total += 3;
        passed += check("v3d_scale_add x", *(uint32_t*)&out[0], 0x40300000, buf);
        passed += check("v3d_scale_add y", *(uint32_t*)&out[1], 0xBF000000, buf);
        passed += check("v3d_scale_add z", *(uint32_t*)&out[2], 0x3FE66666, buf);
    }

    /* matrix_transform_point */
    {
        float mat[12] = {
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            10.0f, 20.0f, 30.0f
        };
        float in_point[3] = { 5.0f, 5.0f, 5.0f };
        float out_point[3];

        matrix_transform_point(mat, in_point, out_point);
        
        total += 3;
        passed += check("mat_transform_pt x", *(uint32_t*)&out_point[0], 0x41C80000, buf);
        passed += check("mat_transform_pt y", *(uint32_t*)&out_point[1], 0x420C0000, buf);
        passed += check("mat_transform_pt z", *(uint32_t*)&out_point[2], 0x42960000, buf);
    }

    /* perpendicular3d */
    {
        float in_vec[3] = { 1.0f, 0.0f, 0.0f };
        float out_vec[3];

        perpendicular3d(in_vec, out_vec);
        
        total += 3;
        passed += check("perp3d x", *(uint32_t*)&out_vec[0], 0x80000000, buf);
        passed += check("perp3d y", *(uint32_t*)&out_vec[1], 0x00000000, buf);
        passed += check("perp3d z", *(uint32_t*)&out_vec[2], 0x3F800000, buf);
    }

    /* angles_to_vector */
    {
        float angles[2] = { 0.785398f, 0.785398f };
        float out_vec[3];

        angles_to_vector(out_vec, angles);
        
        total += 3;
        passed += check("angles2vec x", *(uint32_t*)&out_vec[0], 0x3F000003, buf);
        passed += check("angles2vec y", *(uint32_t*)&out_vec[1], 0x3F000000, buf);
        passed += check("angles2vec z", *(uint32_t*)&out_vec[2], 0x3F3504F1, buf);
    }

    crt_sprintf(buf, "--- TEST_HARNESS_END: %d/%d passed ---\n", passed, total);
    debug_string_to_display(buf, 0);
}
