extern float normalize3d(float *v);
extern void vector3d_scale_add(float *base, float *direction, float scale, float *out);
extern void matrix_transform_point(float *matrix, float *in, float *out);
extern void perpendicular3d(float *in, float *out);
extern void angles_to_vector(float *out, float *angles);

extern void object_placement_data_new(void *placement, int tag_index, int parent_handle);
extern void matrix_inverse(float *src, float *dst);
extern void matrix4x3_multiply(float *a, float *b, float *out);
extern void matrix_from_forward_and_up(float *out, float *forward, float *up);

extern void *csstrncpy(char *destination, const char *source, size_t size);
extern char *csstrtok(char *string, const char *delimiters);
extern void set_random_seed(int seed);
extern float random_math_real(unsigned int *seed);

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
        passed += check("mat_transform_pt z", *(uint32_t*)&out_point[2], 0x42480000, buf);
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

    /* object_placement_data_new: Object Placement Init */
    {
        uint8_t placement[0x88];
        int i;
        for (i = 0; i < sizeof(placement); i++) {
            placement[i] = 0xCC;
        }
        
        object_placement_data_new(placement, 0x1234, -1);
        
        total += 3;
        passed += check("placement tag", *(uint32_t*)&placement[0x00], 0x00001234, buf);
        passed += check("placement scale x", *(uint32_t*)&placement[0x58], 0x3F800000, buf);
        passed += check("placement tail", placement[0x87], 0x0000003F, buf);
    }

    /* matrix_inverse */
    {
        float src_mat[12] = {
            0.0f, -1.0f, 0.0f,
            1.0f,  0.0f, 0.0f,
            0.0f,  0.0f, 1.0f,
            10.0f, 20.0f, 30.0f
        };
        float dst_mat[12];
        int i;
        for (i = 0; i < 12; i++) dst_mat[i] = 0.0f;

        matrix_inverse(src_mat, dst_mat);

        total += 3;
        passed += check("mat_inv m00", *(uint32_t*)&dst_mat[0], 0x00000000, buf);
        passed += check("mat_inv m10", *(uint32_t*)&dst_mat[3], 0x00000000, buf);
        passed += check("mat_inv trans_x", *(uint32_t*)&dst_mat[9], 0x00000000, buf);
    }

    /* matrix4x3_multiply */
    {
        float mat_a[12] = {
            1.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            1.0f, 2.0f, 3.0f
        };
        float mat_b[12] = {
            0.0f, 1.0f, 0.0f,
            -1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            5.0f, 5.0f, 5.0f
        };
        float out_mat[12];

        matrix4x3_multiply(mat_a, mat_b, out_mat);

        total += 3;
        passed += check("mat_mul m01", *(uint32_t*)&out_mat[1], 0x00000000, buf);
        passed += check("mat_mul trans_y", *(uint32_t*)&out_mat[10], 0x40E00000, buf);
        passed += check("mat_mul trans_z", *(uint32_t*)&out_mat[11], 0x40800000, buf);
    }

    /* matrix_from_forward_and_up */
    {
        float forward[3] = { 1.0f, 1.0f, 0.0f }; // Will be normalized implicitly?
        float up[3] = { 0.0f, 0.0f, 1.0f };
        float out_mat[12];

        matrix_from_forward_and_up(out_mat, forward, up);

        total += 3;
        passed += check("mat_fwd_up m00", *(uint32_t*)&out_mat[0], 0x3F800000, buf);
        passed += check("mat_fwd_up m01", *(uint32_t*)&out_mat[1], 0x3F800000, buf);
        passed += check("mat_fwd_up m22", *(uint32_t*)&out_mat[8], 0x00000000, buf);
    }

    /* csstrncpy */
    {
        char dst[16];
        int i;
        for (i = 0; i < sizeof(dst); i++) dst[i] = 0xCC;
        
        csstrncpy(dst, "hello world", 8); // Should copy 8 chars and not null terminate?
        
        total += 2;
        passed += check("csstrncpy last char", dst[7], 0x0000006F, buf);
        passed += check("csstrncpy next char", (uint8_t)dst[8], 0x000000CC, buf);
    }

    /* csstrtok */
    {
        char str[] = "this,is;a\ttest";
        char *tok1 = csstrtok(str, ",;\t");
        char *tok2 = csstrtok(NULL, ",;\t");

        total += 2;
        passed += check("csstrtok t1", tok1 ? tok1[0] : 0, 0x00000074, buf);
        passed += check("csstrtok t2", tok2 ? tok2[0] : 0, 0x00000069, buf);
    }

    /* random_math_real */
    {
        unsigned int local_seed = 1337;
        set_random_seed(local_seed);
        
        float r1 = random_math_real(&local_seed);
        float r2 = random_math_real(&local_seed);

        total += 2;
        passed += check("rand r1", *(uint32_t*)&r1, 0x3F4114C1, buf);
        passed += check("rand r2", *(uint32_t*)&r2, 0x3F0CAC8D, buf);
    }

    crt_sprintf(buf, "--- TEST_HARNESS_END: %d/%d passed ---\n", passed, total);
    debug_string_to_display(buf, 0);
}
