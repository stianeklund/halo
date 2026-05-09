/* Return the number of free bytes available in a circular queue (0x118e90).
 * Computes: buffer_size - used - 1, where used = (write_offset - read_offset),
 * wrapping around via buffer_size when write_offset < read_offset. The -1
 * accounts for the sentinel gap that distinguishes full from empty. */
unsigned int FUN_00118e90(int queue)
{
    int used;

    FUN_00118d70(queue);

    used = *(int *)(queue + 0x0c) - *(int *)(queue + 0x08);
    if (used < 0) {
        used = used + *(int *)(queue + 0x10);
    }
    return *(int *)(queue + 0x10) - used - 1;
}
