import { execFileSync } from "node:child_process"
import { createHash } from "node:crypto"
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs"
import { dirname, join } from "node:path"

const DEBUG_RE = /\b(regression|crash|crashes|crashed|fault|page fault|access[_ -]?violation|assert|hang|freeze|deadlock|wrong|broken|bug|visual|tint|color|invisible|missing|no[- ]?draw|cull|spawn|position|build failure|deploy failure|symbol absent|eip|cr2|trap frame|rasterizer)\b/i
const EXCEPTION_RE = /\b(EXCEPTION|ACCESS[_ -]?VIOLATION|trap frame|halt in|EIP\s*[:=]|CR2\s*[:=]|tag_groups\.c,#\d+)\b|^\s*\[[0-9]+\]\s*[:=]?\s*(?:0x)?[0-9a-f]{5,8}\b/im
const MAX_MESSAGE_CHARS = 4200
const MAX_SYMBOLIZER_CHARS = 3600

const SKILL_RULES = [
  {
    re: /\b(lift|lifting|ported|porting|re[- ]?lift|FUN_[0-9a-f]{8}|0x[0-9a-f]{5,}|ghidra|decompil|cachebeta|kb\.json)\b|@<[a-z]+>/i,
    skills: ["halo-xbox-re", "halo-re-lift"],
    why: "Halo RE/lift workflow, binary evidence rules, ABI and kb.json discipline",
  },
  {
    re: /\b(call[ -]?site|add esp|push|fstp|x87|cross[- ]?product|_ftol2|_chkstk|__seh|_allmul|intrinsic|decompiler trap|ghidra.*wrong)\b/i,
    skills: ["lift-decompiler-traps", "lift-arg-hazards"],
    why: "call-site verification, cdecl cleanup tells, FSTP float args, intrinsics, register aliasing",
  },
  {
    re: /\b(register arg|reg arg|in_eax|in_ecx|in_edx|in_esi|in_edi|callee regs?|unported callee|xcall|missing @)\b|@</i,
    skills: ["check-callee-regs", "lift-arg-hazards"],
    why: "implicit @<reg> ABI hazards and caller register setup checks",
  },
  {
    re: /\b(_chkstk|stack frame|frame size|buffer size|undersized buffer|local_[0-9a-f]+|memset|memcpy|stack alias|buffer alias)\b|&local_/i,
    skills: ["lift-frame-hazards", "lift-decompiler-traps"],
    why: "stack-frame sizing, contiguous buffer rules, local_XX buffer-alias reads",
  },
  {
    re: /\b(vc71|vc71_verify|low[- ]?match|match percent|structural ceiling|objdiff|permuter|permute|permutation campaign)\b|85%|98%/i,
    skills: ["halo-verify-debug", "lift-score-improve", "permuter-campaign"],
    why: "verification lanes, score recovery before declaring a ceiling, safe permuter campaign rules",
  },
  {
    re: DEBUG_RE,
    skills: ["debug", "crash-triage", "lift-crash-signals"],
    why: "runtime symptom router, crash signal table, toggle-bisect and liveness gates",
  },
  {
    re: /\b(wrong color|yellow|white tint|invisible|missing geometry|no effect|does nothing|wrong position|wrong scalar|silent bug|visual regression|behavioral regression)\b/i,
    skills: ["lift-silent-bugs", "bug-hunt"],
    why: "non-crashing correctness checks and automated hazard/silent-bug scans",
  },
  {
    re: /\b(bug[- ]?hunt|hazard scan|check_lift_hazards|before deploy|pre[- ]?deploy|pre[- ]?commit|safety scan|audit)\b/i,
    skills: ["bug-hunt"],
    why: "tiered automated checks after edits, before deploy, and before commit",
  },
  {
    re: /\b(input replay|deterministic input|capture scenario|halorec|trajectory|a\/b|ab check|behavior_diff|aa check)\b/i,
    skills: ["input-replay-testing", "ab-trajectory-testing"],
    why: "deterministic input replay and A/B trajectory regression oracle",
  },
  {
    re: /\b(xemu|qmp|gdb|screenshot|serial|memory dump|state snapshot)\b/i,
    skills: ["debug-xemu"],
    why: "xemu QMP/GDB/screenshot/live-memory probing workflow",
  },
]

function textFromPart(part) {
  if (!part) return ""
  if (typeof part === "string") return part
  if (typeof part.text === "string") return part.text
  if (typeof part.content === "string") return part.content
  return ""
}

function textFromMessage(message) {
  if (!message) return ""
  if (typeof message.content === "string") return message.content
  if (Array.isArray(message.content)) return message.content.map(textFromPart).join("\n")
  if (Array.isArray(message.parts)) return message.parts.map(textFromPart).join("\n")
  return ""
}

function latestUserText(messages) {
  if (!Array.isArray(messages)) return ""
  for (let i = messages.length - 1; i >= 0; i--) {
    const message = messages[i]
    if (message && (message.role === "user" || message.role === "human")) {
      const text = textFromMessage(message).trim()
      if (text) return text
    }
  }
  return ""
}

function statePath(root) {
  return join(root, ".opencode", "memory-router-state.json")
}

function recentlyRan(root, prompt, windowSeconds = 180) {
  const digest = createHash("sha256").update(prompt).digest("hex").slice(0, 16)
  const path = statePath(root)
  let state = {}
  try {
    if (existsSync(path)) state = JSON.parse(readFileSync(path, "utf8"))
  } catch {
    state = {}
  }
  const now = Date.now() / 1000
  if (typeof state[digest] === "number" && now - state[digest] < windowSeconds) return true
  state[digest] = now
  for (const [key, value] of Object.entries(state)) {
    if (typeof value !== "number" || now - value > 86400) delete state[key]
  }
  try {
    mkdirSync(dirname(path), { recursive: true })
    writeFileSync(path, JSON.stringify(state, null, 2))
  } catch {
    // Best effort only; never break opencode because the router could not write state.
  }
  return false
}

function runPriorFixes(root, prompt) {
  try {
    return execFileSync(
      "python3",
      ["tools/memory/prior_fixes.py", prompt, "--limit", "6", "--max-commits", "160"],
      { cwd: root, encoding: "utf8", timeout: 18000, stdio: ["ignore", "pipe", "ignore"] },
    ).trim()
  } catch {
    return ""
  }
}

function buildPriorFixMessage(report) {
  let body = report
  if (body.length > MAX_MESSAGE_CHARS) body = `${body.slice(0, MAX_MESSAGE_CHARS - 80).trim()}\n... (prior-fix lookup truncated)`
  return `[prior-fix-router] This looks like a debug/regression task. Use these local leads before investigating. Matches are hypotheses only; confirm against binary/disassembly/runtime evidence before fixing.\n\n${body}`
}

function matchingSkillRules(prompt) {
  return SKILL_RULES.filter((rule) => rule.re.test(prompt)).slice(0, 6)
}

function buildSkillMessage(matches) {
  const seen = new Set()
  const skills = []
  for (const match of matches) {
    for (const skill of match.skills) {
      if (!seen.has(skill)) {
        seen.add(skill)
        skills.push(skill)
      }
    }
  }
  const routes = matches.map((match) => `- ${match.skills.map((name) => `\`${name}\``).join(" + ")}: ${match.why}`).join("\n")
  return `[skill-router] Local Halo trigger words matched. Before acting, load/use these skills if the Skill tool exposes them; otherwise read \`.claude/skills/<skill>/SKILL.md\` and follow its checklist. When delegating to a subagent, name these skills in the brief.\n\nRecommended skills: ${skills.map((name) => `\`${name}\``).join(", ")}\n\nMatched routes:\n${routes}`
}

function runExceptionSymbolizer(root, prompt) {
  try {
    return execFileSync(
      "python3",
      ["tools/xbox/symbolize_exception.py", "--stdin"],
      { cwd: root, input: prompt, encoding: "utf8", timeout: 12000, stdio: ["pipe", "pipe", "ignore"] },
    ).trim()
  } catch {
    return ""
  }
}

function buildExceptionSymbolizerMessage(report) {
  let body = report
  if (body.length > MAX_SYMBOLIZER_CHARS) body = `${body.slice(0, MAX_SYMBOLIZER_CHARS - 80).trim()}\n... (symbolizer output truncated)`
  return `[exception-symbolizer] Pasted exception/backtrace text detected. Use this fresh PE-export/kb.json symbolization before forming hypotheses; do not use \`build/halo.map\` for runtime crashes.\n\n\`\`\`text\n${body}\n\`\`\``
}

function injectSystemMessage(output, message) {
  if (Array.isArray(output.messages)) {
    output.messages.unshift({ role: "system", content: message })
    return true
  }
  if (typeof output.system === "string") {
    output.system = `${output.system}\n\n${message}`
    return true
  }
  if (Array.isArray(output.context)) {
    output.context.push(message)
    return true
  }
  return false
}

function maybeInjectPriorFixes(root, prompt, output) {
  if (!DEBUG_RE.test(prompt)) return
  if (recentlyRan(root, `prior:${prompt}`)) return
  const report = runPriorFixes(root, prompt)
  if (!report) return
  injectSystemMessage(output, buildPriorFixMessage(report))
}

function maybeInjectSkillRouter(root, prompt, output) {
  const matches = matchingSkillRules(prompt)
  if (matches.length === 0) return
  if (recentlyRan(root, `skill:${prompt}`)) return
  injectSystemMessage(output, buildSkillMessage(matches))
}

function maybeInjectExceptionSymbolizer(root, prompt, output) {
  if (!EXCEPTION_RE.test(prompt)) return
  if (recentlyRan(root, `symbolize:${prompt}`)) return
  const report = runExceptionSymbolizer(root, prompt)
  if (!report) return
  injectSystemMessage(output, buildExceptionSymbolizerMessage(report))
}

function maybeInject(root, input, output) {
  const messages = output.messages || input.messages || input.parts || []
  const prompt = latestUserText(messages)
  if (!prompt) return
  maybeInjectSkillRouter(root, prompt, output)
  maybeInjectPriorFixes(root, prompt, output)
  maybeInjectExceptionSymbolizer(root, prompt, output)
}

export const HaloMemoryRouter = async ({ worktree, directory }) => {
  const root = worktree || directory || process.cwd()
  return {
    "experimental.chat.messages.transform": async (input, output) => {
      maybeInject(root, input || {}, output || {})
    },
    "experimental.chat.system.transform": async (input, output) => {
      maybeInject(root, input || {}, output || {})
    },
  }
}
