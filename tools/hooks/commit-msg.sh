#!/bin/sh
# commit-msg hook: block GitHub issue/PR autolink references ("#<number>") in
# commit messages.
#
# Why: GitHub auto-links a bare "#123" token to issue/PR 123 in the repository
# network. When our commits are pushed to the upstream fork (halo-re/halo), every
# such token posts a "referenced this issue" cross-reference on that issue,
# spamming its timeline with unrelated lift/debug commits. See the 2026-07-08
# cleanup that stripped these from history.
#
# What it catches: a "#" followed by one or more digits at a word boundary
# (e.g. "#2", "issue #2", "PR #15)"). It deliberately does NOT match:
#   - "#define" / "#include"      (# followed by a letter)
#   - "#2a2a2a" hex colors        (digits followed by a letter -> not an autolink)
#   - "crash#2" / "bug#2740"      (# preceded by a letter -> not autolinked)
#   - git comment lines ("# ...") which git strips before committing
#
# Bypass (rare, genuinely intentional reference): git commit --no-verify

msg_file="$1"
[ -n "$msg_file" ] && [ -f "$msg_file" ] || exit 0

# Drop git comment lines so status/diff hints and scissors don't false-positive,
# then look for autolink tokens in what remains.
offending=$(grep -vE '^[[:space:]]*#' "$msg_file" 2>/dev/null \
  | grep -nE '(^|[^A-Za-z0-9])#[0-9]+([^A-Za-z0-9]|$)')

if [ -n "$offending" ]; then
  echo "commit-msg: refusing commit — message contains GitHub autolink reference(s) '#<number>':" >&2
  echo "$offending" | sed 's/^/    /' >&2
  echo "" >&2
  echo "  These auto-link to issues/PRs and spam the upstream fork's timeline." >&2
  echo "  Fix: drop the '#' and write the number plainly (e.g. 'issue 2'), or paste the full URL." >&2
  echo "  Intentional reference? Bypass with:  git commit --no-verify" >&2
  exit 1
fi

exit 0
