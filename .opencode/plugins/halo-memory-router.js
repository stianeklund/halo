import { execFileSync } from "node:child_process"
import { createHash } from "node:crypto"
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs"
import { dirname, join } from "node:path"

const DEBUG_RE = /\b(regression|crash|crashes|crashed|fault|page fault|access[_ -]?violation|assert|hang|freeze|deadlock|wrong|broken|bug|visual|tint|color|invisible|missing|no[- ]?draw|cull|spawn|position|build failure|deploy failure|symbol absent|eip|cr2|trap frame|rasterizer)\b/i
const MAX_MESSAGE_CHARS = 4200

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

function buildMessage(report) {
  let body = report
  if (body.length > MAX_MESSAGE_CHARS) body = `${body.slice(0, MAX_MESSAGE_CHARS - 80).trim()}\n... (prior-fix lookup truncated)`
  return `[prior-fix-router] This looks like a debug/regression task. Use these local leads before investigating. Matches are hypotheses only; confirm against binary/disassembly/runtime evidence before fixing.\n\n${body}`
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

function maybeInject(root, input, output) {
  const messages = output.messages || input.messages || input.parts || []
  const prompt = latestUserText(messages)
  if (!prompt || !DEBUG_RE.test(prompt)) return
  if (recentlyRan(root, prompt)) return
  const report = runPriorFixes(root, prompt)
  if (!report) return
  injectSystemMessage(output, buildMessage(report))
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
