#!/usr/bin/env bash
# Pre-commit gate: tag_block_get_element element-size consistency.
#
# `tag_block_get_element(block, index, element_size)` reads element `index` at
# `block + index * element_size`. The element size is a property of the tag
# block, so every call site addressing the SAME block -- same tag group, same
# offset within the tag -- must pass the SAME size. One wrong literal reads
# mid-element for any index != 0.
#
# That is a bug we shipped: particle_systems.c passed 0x6c for the 'obje'
# particle_systems block at +0x140 where the original pushes 0x48, and
# contrails.c / particles.c both already used 0x48. 0x6c was the marker_buf
# entry stride from a few lines below -- an unrelated number. Every attachment
# index != 0 produced a bogus string_id, so the marker lookup failed or
# matched the wrong marker.
#
# VC71 was blind to it (71.8% before and after the fix -- one immediate in a
# 254-instruction function is aligned away by the LCS), and it only shows up
# at runtime once content uses a non-zero index. The check is cross-site
# consistency, which catches it statically for ~1s.
#
# Attribution is by VARIABLE (which variable received the tag_get result), not
# by proximity, and buckets whose tag group cannot be resolved are skipped
# entirely -- so the whole-repo scan reports zero false positives over 647
# call sites while still flagging the real conflict.
#
# Runs only when a .c file is staged.

REPO_ROOT="$(git rev-parse --show-toplevel)"

mapfile -t STAGED_C < <(git diff --cached --name-only --diff-filter=ACMR | grep '\.c$')
[ ${#STAGED_C[@]} -eq 0 ] && exit 0

# Whole-repo scan by design: a conflict is between TWO sites, and the other
# one is usually in a file this commit does not touch.
python3 "$REPO_ROOT/tools/audit/check_tag_block_sizes.py" --check \
    >/tmp/tag-block-sizes-check.txt 2>&1
RC=$?
if [ $RC -ne 0 ]; then
    echo ""
    echo "pre-commit BLOCKED: tag_block_get_element element-size conflict."
    grep -A6 "CONFLICT" /tmp/tag-block-sizes-check.txt | head -40
    echo ""
    echo "One tag block has ONE element size. Check the original's"
    echo "\`PUSH <size>\` at the call site before changing either literal."
    echo "If the offset is genuinely two different blocks, record it:"
    echo "  python3 tools/audit/check_tag_block_sizes.py --update-baseline"
    echo "Bypass: git commit --no-verify (emergency only)."
fi
exit $RC
