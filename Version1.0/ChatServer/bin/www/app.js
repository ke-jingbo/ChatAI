"use strict";

const $ = (id) => document.getElementById(id);
const icons = {
  panel: '<rect x="3" y="4" width="18" height="16" rx="3"/><path d="M9 4v16"/>',
  folder:
    '<path d="M3 7a2 2 0 0 1 2-2h5l2 2h7a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2Z"/>',
  edit: '<path d="M12 4H6a2 2 0 0 0-2 2v12a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-6M14 5l3-3 5 5-3 3-8 8-5 1 1-5Z"/>',
  rename: '<path d="m4 16.5-.7 3.2 3.2-.7L18.8 6.7a2.2 2.2 0 0 0-3.1-3.1Z"/><path d="m14.5 5.7 3.8 3.8"/>',
  search: '<circle cx="10" cy="10" r="6"/><path d="m15 15 5 5"/>',
  refresh:
    '<path d="M20 4v6h-6M4 20v-6h6M4 10a8 8 0 0 1 14-5l2 5M20 14a8 8 0 0 1-14 5l-2-5"/>',
  trash: '<path d="M4 6h16M9 6V3h6v3M6 6l1 15h10l1-15M10 10v7M14 10v7"/>',
  plus: '<path d="M12 5v14M5 12h14"/>',
  close: '<path d="m6 6 12 12M6 18 18 6"/>',
  arrow: '<path d="M12 19V5m-5 5 5-5 5 5"/>',
  chat: '<path d="M4 5h16v12H9l-5 4V5Z"/>',
  sliders:
    '<path d="M4 7h6m4 0h6M4 17h10m4 0h2"/><circle cx="12" cy="7" r="2"/><circle cx="16" cy="17" r="2"/>',
  sun: '<circle cx="12" cy="12" r="4"/><path d="M12 2v2m0 16v2M2 12h2m16 0h2M5 5l1 1m12 12 1 1M5 19l1-1M18 6l1-1"/>',
  moon: '<path d="M20 15A8 8 0 0 1 9 4 8 8 0 1 0 20 15Z"/>',
  spark:
    '<path d="M12 3c2-2 5 0 5 2 4-1 6 3 4 6 3 3 0 7-3 7 0 4-5 5-7 2-4 2-7-1-6-4-4-2-3-6 0-8-1-3 4-6 7-5Z"/><path d="m8 10 2 2-2 2m5 1h3"/>',
};
function icon(name) {
  return (
    '<svg viewBox="0 0 24 24" aria-hidden="true">' + icons[name] + "</svg>"
  );
}
document.querySelectorAll("[data-icon]").forEach((el) => {
  el.innerHTML = icon(el.dataset.icon);
});
const storage = {
  get(key) {
    try {
      return localStorage.getItem(key);
    } catch {
      return null;
    }
  },
  set(key, value) {
    try {
      localStorage.setItem(key, value);
    } catch {
      /* Private browsing can block storage. */
    }
  },
};
const state = {
  base: storage.get("chatserver-api-base") || "",
  models: [],
  sessions: [],
  current: "",
  model: "",
  busy: false,
  loading: false,
  online: false,
  configs: new Map(),
  draftConfig: null,
  drafts: new Map(),
  menu: "",
  rename: null,
  deletion: null,
};
const defaults = {
  temperature: 0.8,
  max_tokens: 393216,
  think: true,
  reasoning_effort: "high",
};
const list = (value) => (Array.isArray(value) ? value : []);
const current = () =>
  state.sessions.find((s) => s.session_id === state.current);
const title = (s) =>
  String(s?.session_name || "new session")
    .replace(/\s+/g, " ")
    .trim();
function asDate(timestamp) {
  const value = Number(timestamp);
  if (!Number.isFinite(value) || value <= 0) return null;
  return new Date(value < 1e12 ? value * 1000 : value);
}
function formatTimestamp(timestamp, withDay = false) {
  const date = asDate(timestamp);
  if (!date || Number.isNaN(date.getTime())) return "";
  const time = date.toLocaleTimeString("zh-CN", {
    hour: "2-digit",
    minute: "2-digit",
  });
  if (!withDay) return time;
  const today = new Date();
  const day = (value) =>
    new Date(value.getFullYear(), value.getMonth(), value.getDate()).getTime();
  const difference = Math.round((day(today) - day(date)) / 86400000);
  if (difference === 0) return "今天 " + time;
  if (difference === 1) return "昨天 " + time;
  return (
    date.toLocaleDateString("zh-CN", { month: "2-digit", day: "2-digit" }) +
    " " +
    time
  );
}
let toastTimer;
function toast(message) {
  $("toast").textContent = message;
  $("toast").classList.add("visible");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => $("toast").classList.remove("visible"), 3500);
}
function connection(online, label) {
  state.online = online;
  $("status").textContent = label;
  $("status-dot").classList.toggle("online", online);
}

// Every mutation is serialized so responses cannot update a different session.
async function action(label, work) {
  if (state.busy) return;
  state.busy = true;
  $("activity").textContent = label;
  controls();
  try {
    await work();
  } catch (error) {
    toast(error.message || "请求失败，请重试");
  } finally {
    state.busy = false;
    $("activity").textContent = "";
    controls();
  }
}
function controls() {
  for (const id of [
    "new-chat",
    "refresh",
    "clear",
    "name-new",
    "rename-title",
    "model",
    "model-toggle",
    "send",
    "retry-history",
    "connection",
    "stream",
  ])
    $(id).disabled = state.busy;
  document
    .querySelectorAll(
      "#sessions button, dialog button, dialog input, #parameters input, #parameters button",
    )
    .forEach((el) => {
      el.disabled = state.busy;
    });
  $("clear").disabled = state.busy || !state.sessions.length;
  $("rename-title").disabled = state.busy || !state.current;
  $("model-toggle").disabled = state.busy || !state.models.length;
  $("message").disabled = state.busy;
  $("send").disabled =
    state.busy || !state.models.length || !$("message").value.trim();
  $("send-state").textContent = state.busy
    ? "正在处理，请稍候…"
    : "Enter 发送 · Shift + Enter 换行";
}
async function api(path, method = "GET", body) {
  const controller = new AbortController();
  const timer = setTimeout(
    () => controller.abort(),
    path === "/api/message" ? 180000 : 30000,
  );
  try {
    const response = await fetch(state.base + path, {
      method,
      signal: controller.signal,
      headers: body === undefined ? {} : { "Content-Type": "application/json" },
      body: body === undefined ? undefined : JSON.stringify(body),
    });
    const raw = await response.text();
    let data;
    try {
      data = JSON.parse(raw);
    } catch {
      throw new Error(raw.slice(0, 160) || "服务器返回了无效响应");
    }
    if (!response.ok || data.success === false)
      throw new Error(data.message || "请求失败：" + response.status);
    return data.data;
  } catch (error) {
    if (error.name === "AbortError")
      throw new Error("请求超时，请刷新确认服务器状态后再试");
    throw error;
  } finally {
    clearTimeout(timer);
  }
}
function theme(value) {
  document.documentElement.dataset.theme = value;
  storage.set("chatserver-appearance", value);
  $("theme").innerHTML = icon(value === "dark" ? "sun" : "moon");
  const label = value === "dark" ? "切换亮色主题" : "切换暗色主题";
  $("theme").title = label;
  $("theme").setAttribute("aria-label", label);
}
function closeSidebar() {
  document.body.classList.remove("mobile-open");
  $("backdrop").hidden = true;
}
function closePanel() {
  $("model-panel").hidden = true;
  $("model-toggle").setAttribute("aria-expanded", "false");
}
function renderSessions() {
  const query = $("search").value.trim().toLowerCase();
  const sessions = state.sessions.filter((s) =>
    title(s).toLowerCase().includes(query),
  );
  $("sessions").replaceChildren();
  if (!sessions.length) {
    const empty = document.createElement("p");
    empty.className = "empty-list";
    empty.textContent = query ? "没有匹配的会话" : "还没有会话，随时开始。";
    $("sessions").append(empty);
  }
  for (const session of sessions) {
    const item = document.createElement("div");
    item.className =
      "session" + (session.session_id === state.current ? " active" : "");
    const row = document.createElement("div");
    row.className = "session-row";
    const select = document.createElement("button");
    select.className = "session-select";
    select.textContent = title(session);
    select.title = title(session);
    if (session.session_id === state.current)
      select.setAttribute("aria-current", "page");
    select.onclick = () =>
      action("载入会话…", () => selectSession(session.session_id));
    const more = document.createElement("button");
    more.className = "icon session-more";
    more.textContent = "···";
    more.setAttribute("aria-label", title(session) + "的操作");
    more.setAttribute(
      "aria-expanded",
      String(state.menu === session.session_id),
    );
    more.onclick = () => {
      state.menu = state.menu === session.session_id ? "" : session.session_id;
      renderSessions();
    };
    row.append(select, more);
    const updateTimestamp = session.update_time || session.start_time;
    const sessionTime = document.createElement("time");
    sessionTime.className = "session-time";
    sessionTime.textContent = formatTimestamp(updateTimestamp, true);
    const updateDate = asDate(updateTimestamp);
    if (updateDate) sessionTime.dateTime = updateDate.toISOString();
    sessionTime.title = "更新时间：" + sessionTime.textContent;
    item.append(row, sessionTime);
    if (state.menu === session.session_id) {
      const actions = document.createElement("div");
      actions.className = "session-actions";
      const remove = document.createElement("button");
      remove.textContent = "删除会话";
      remove.className = "delete";
      remove.onclick = () => confirmDelete(session);
      actions.append(remove);
      item.append(actions);
    }
    $("sessions").append(item);
  }
  controls();
}
function updateHeader() {
  const currentTitle = state.current ? title(current()) : "新对话";
  $("title").textContent = currentTitle;
  $("rename-title-text").textContent = currentTitle;
  $("context-text").textContent = currentTitle;
  $("rename-title").hidden = !state.current;
  $("rename-title").title = state.current ? "更改会话名" : "请先打开一个会话";
  $("model-label").textContent = state.model || "选择模型";
  $("model").value = state.model;
}
function cacheSessionConfigs() {
  for (const session of state.sessions) {
    const config = session.model_config;
    if (!config || !session.model) continue;
    state.configs.set(session.model, {
      ...config,
      max_tokens: Number(config.max_tokens),
    });
  }
}
async function refreshSessions() {
  state.sessions = list(await api("/api/sessions"));
  if (state.current && !current()) home();
  cacheSessionConfigs();
  updateHeader();
  renderSessions();
}
function saveDraft() {
  state.drafts.set(state.current, $("message").value);
}
function home() {
  saveDraft();
  state.current = "";
  state.menu = "";
  $("message").value = state.drafts.get("") || "";
  $("messages").replaceChildren();
  $("welcome").hidden = false;
  $("history-error").hidden = true;
  closeSidebar();
  closePanel();
  updateHeader();
  renderSessions();
  resize();
}
async function selectSession(id) {
  saveDraft();
  state.current = id;
  state.model = current().model;
  $("message").value = state.drafts.get(id) || "";
  $("messages").replaceChildren();
  $("welcome").hidden = true;
  $("history-error").hidden = true;
  closeSidebar();
  closePanel();
  updateHeader();
  renderSessions();
  resize();
  try {
    const history = list(
      await api("/api/session/" + encodeURIComponent(id) + "/history"),
    );
    history.sort(
      (a, b) =>
        Number(a.timestamp) - Number(b.timestamp) ||
        String(a.message_id).localeCompare(String(b.message_id)),
    );
    for (const message of history)
      addMessage(message.role, message.content, message.timestamp);
    $("welcome").hidden = history.length > 0;
    bottom();
  } catch (error) {
    $("history-error").hidden = false;
    throw error;
  }
}
function openName(session = null) {
  if (state.busy) return;
  state.rename = session?.session_id || null;
  $("name-title").textContent = session ? "重命名会话" : "创建会话";
  $("name").value = session ? title(session) : "new session";
  $("name-note").hidden = Boolean(session);
  $("name-dialog").showModal();
  $("name").focus();
  $("name").select();
}
async function createSession(name = "new session") {
  if (!state.model) throw new Error("请先选择可用模型");
  const config = state.draftConfig || state.configs.get(state.model) || defaults;
  const data = await api("/api/session", "POST", {
    model: {
      model_name: state.model,
      temperature: Number(config.temperature),
      max_tokens: Number(config.max_tokens),
      think: Boolean(config.think),
      reasoning_effort: config.reasoning_effort,
    },
    session_name: name,
  });
  if (!data?.session_id) throw new Error("创建响应缺少会话 ID");
  const createdModel = data.model?.model || state.model;
  const session = {
    session_id: data.session_id,
    session_name: data.session_name || name,
    model: createdModel,
    message_count: 0,
  };
  state.sessions.unshift(session);
  state.current = session.session_id;
  state.model = createdModel;
  state.configs.set(createdModel, data.model || config);
  state.draftConfig = null;
  $("messages").replaceChildren();
  $("welcome").hidden = false;
  $("history-error").hidden = true;
  updateHeader();
  renderSessions();
}
function confirmDelete(session) {
  if (state.busy) return;
  state.deletion = session?.session_id || "*";
  $("confirm-title").textContent = session
    ? "删除这个会话？"
    : "清空全部会话？";
  $("confirm-text").textContent = session
    ? "“" + title(session) + "”及其历史消息将永久删除。"
    : "所有会话及历史消息将永久删除，此操作无法撤销。";
  $("confirm-dialog").showModal();
}

// Model output is rendered through DOM text nodes. No model HTML is executed.
function mathText(value) {
  return value
    .replace(/\\Theta\b/g, "Θ")
    .replace(/\\Omega\b/g, "Ω")
    .replace(/\\times\b/g, "×")
    .replace(/\\cdot\b/g, "·")
    .replace(/\\leq?\b/g, "≤")
    .replace(/\\geq?\b/g, "≥")
    .replace(/\\infty\b/g, "∞")
    .replace(/\\log\b/g, "log");
}

function appendInlineMath(parent, expression) {
  const math = document.createElement("span");
  math.className = "math-inline";
  const source = mathText(expression);
  const script = /(\^|_)(?:\{([^{}]+)\}|([A-Za-z0-9+\-=]+))/g;
  let cursor = 0,
    match;
  while ((match = script.exec(source))) {
    math.append(document.createTextNode(source.slice(cursor, match.index)));
    const element = document.createElement(match[1] === "^" ? "sup" : "sub");
    element.textContent = match[2] || match[3];
    math.append(element);
    cursor = script.lastIndex;
  }
  math.append(document.createTextNode(source.slice(cursor)));
  parent.append(math);
}

function inline(parent, text) {
  const pieces = String(text).split(
    /(`[^`\n]+`|\\\([^\n]*?\\\)|\*\*[^*\n]+\*\*|__[^_\n]+__|\*[^*\n]+\*|_[^_\n]+_)/g,
  );
  for (const piece of pieces) {
    if (!piece) continue;
    if (piece.startsWith("\\(") && piece.endsWith("\\)")) {
      appendInlineMath(parent, piece.slice(2, -2));
    } else if (
      (piece.startsWith("**") && piece.endsWith("**")) ||
      (piece.startsWith("__") && piece.endsWith("__"))
    ) {
      const el = document.createElement("strong");
      el.textContent = piece.slice(2, -2);
      parent.append(el);
    } else if (piece.startsWith("`") && piece.endsWith("`")) {
      const el = document.createElement("code");
      el.textContent = piece.slice(1, -1);
      parent.append(el);
    } else if (
      (piece.startsWith("*") && piece.endsWith("*")) ||
      (piece.startsWith("_") && piece.endsWith("_"))
    ) {
      const el = document.createElement("em");
      el.textContent = piece.slice(1, -1);
      parent.append(el);
    } else parent.append(document.createTextNode(piece));
  }
}

async function copyText(value) {
  if (navigator.clipboard && window.isSecureContext) {
    await navigator.clipboard.writeText(value);
    return;
  }
  const input = document.createElement("textarea");
  input.value = value;
  input.style.position = "fixed";
  input.style.opacity = "0";
  document.body.append(input);
  input.select();
  const ok = document.execCommand("copy");
  input.remove();
  if (!ok) throw new Error("copy failed");
}

function appendCodeBlock(target, codeText, info, closed) {
  const language = info.trim().split(/\s+/, 1)[0].replace(/[^\w.+#-]/g, "");
  const block = document.createElement("section");
  block.className = "code-block" + (closed ? "" : " streaming");

  const header = document.createElement("div");
  header.className = "code-header";
  const label = document.createElement("span");
  label.className = "code-language";
  label.textContent = language || "代码";
  const copy = document.createElement("button");
  copy.type = "button";
  copy.className = "code-copy";
  copy.textContent = "复制";
  copy.title = "复制代码";
  copy.setAttribute("aria-label", "复制代码");
  copy.onclick = async () => {
    try {
      await copyText(codeText);
      copy.textContent = "已复制";
      setTimeout(() => {
        copy.textContent = "复制";
      }, 1600);
    } catch {
      toast("复制失败，请选择代码手动复制");
    }
  };
  header.append(label);
  if (!closed) {
    const status = document.createElement("span");
    status.className = "code-status";
    status.textContent = "生成中";
    header.append(status);
  }
  header.append(copy);

  const pre = document.createElement("pre");
  const code = document.createElement("code");
  if (language) code.className = "language-" + language.toLowerCase();
  code.textContent = codeText;
  pre.append(code);
  block.append(header, pre);
  target.append(block);
}

function splitTableRow(line) {
  const source = String(line).trim();
  if (!source.includes("|")) return null;

  const cells = [];
  let cell = "";
  let inCode = false;
  let hasSeparator = false;
  const start = source.startsWith("|") ? 1 : 0;
  let end = source.length;

  if (source.endsWith("|")) {
    let backslashes = 0;
    for (
      let index = source.length - 2;
      index >= 0 && source[index] === "\\";
      index -= 1
    )
      backslashes += 1;
    if (backslashes % 2 === 0) end -= 1;
  }

  for (let index = start; index < end; index += 1) {
    const character = source[index];
    if (character === "\\" && source[index + 1] === "|") {
      cell += "|";
      index += 1;
    } else if (character === "`") {
      inCode = !inCode;
      cell += character;
    } else if (character === "|" && !inCode) {
      cells.push(cell.trim());
      cell = "";
      hasSeparator = true;
    } else {
      cell += character;
    }
  }
  cells.push(cell.trim());
  return hasSeparator || source.startsWith("|") || end < source.length
    ? cells
    : null;
}

function tableAt(lines, index) {
  if (index + 1 >= lines.length) return null;
  const headers = splitTableRow(lines[index]);
  const delimiters = splitTableRow(lines[index + 1]);
  if (!headers || !delimiters || headers.length !== delimiters.length)
    return null;

  const alignments = delimiters.map((cell) => {
    const marker = cell.replace(/\s+/g, "");
    if (!/^:?-{3,}:?$/.test(marker)) return null;
    if (marker.startsWith(":") && marker.endsWith(":")) return "center";
    if (marker.endsWith(":")) return "right";
    return "left";
  });
  return alignments.every(Boolean) ? { headers, alignments } : null;
}

function appendTable(target, headers, alignments, rows) {
  const wrapper = document.createElement("div");
  wrapper.className = "markdown-table-wrap";
  const table = document.createElement("table");
  const head = document.createElement("thead");
  const headRow = document.createElement("tr");

  headers.forEach((value, index) => {
    const cell = document.createElement("th");
    cell.className = "align-" + alignments[index];
    inline(cell, value);
    headRow.append(cell);
  });
  head.append(headRow);
  table.append(head);

  if (rows.length) {
    const body = document.createElement("tbody");
    rows.forEach((values) => {
      const row = document.createElement("tr");
      alignments.forEach((alignment, index) => {
        const cell = document.createElement("td");
        cell.className = "align-" + alignment;
        inline(cell, values[index] || "");
        row.append(cell);
      });
      body.append(row);
    });
    table.append(body);
  }

  wrapper.append(table);
  target.append(wrapper);
}

function markdown(target, text) {
  target.replaceChildren();
  const lines = String(text).replace(/\r\n?/g, "\n").split("\n");
  const fenceAt = (line) => line.match(/^ {0,3}(`{3,}|~{3,})\s*(.*)$/);
  const startsBlock = (line) =>
    Boolean(
      fenceAt(line) ||
        /^ {0,3}#{1,6}\s+/.test(line) ||
        /^\s*[-+*]\s+/.test(line) ||
        /^\s*\d+[.)]\s+/.test(line) ||
        /^\s*>\s?/.test(line) ||
        /^ {0,3}([-*_])(?:\s*\1){2,}\s*$/.test(line),
    );

  let index = 0;
  while (index < lines.length) {
    const line = lines[index];
    if (!line.trim()) {
      index += 1;
      continue;
    }

    const fence = fenceAt(line);
    if (fence) {
      const marker = fence[1];
      const closing = new RegExp(
        "^ {0,3}" + marker[0] + "{" + marker.length + ",}\\s*$",
      );
      const codeLines = [];
      index += 1;
      while (index < lines.length && !closing.test(lines[index])) {
        codeLines.push(lines[index]);
        index += 1;
      }
      const closed = index < lines.length;
      if (closed) index += 1;
      appendCodeBlock(target, codeLines.join("\n"), fence[2], closed);
      continue;
    }

    const heading = line.match(/^ {0,3}(#{1,6})\s+(.+)$/);
    if (heading) {
      const element = document.createElement("h" + heading[1].length);
      inline(element, heading[2].replace(/\s+#+\s*$/, ""));
      target.append(element);
      index += 1;
      continue;
    }

    const table = tableAt(lines, index);
    if (table) {
      const rows = [];
      index += 2;
      while (index < lines.length && lines[index].trim()) {
        const cells = splitTableRow(lines[index]);
        if (!cells) break;
        rows.push(cells.slice(0, table.headers.length));
        index += 1;
      }
      appendTable(target, table.headers, table.alignments, rows);
      continue;
    }

    if (/^ {0,3}([-*_])(?:\s*\1){2,}\s*$/.test(line)) {
      target.append(document.createElement("hr"));
      index += 1;
      continue;
    }

    const listItem = line.match(/^\s*([-+*]|\d+[.)])\s+(.+)$/);
    if (listItem) {
      const ordered = /^\d/.test(listItem[1]);
      const list = document.createElement(ordered ? "ol" : "ul");
      while (index < lines.length) {
        const item = lines[index].match(/^\s*([-+*]|\d+[.)])\s+(.+)$/);
        if (!item || /^\d/.test(item[1]) !== ordered) break;
        const listElement = document.createElement("li");
        inline(listElement, item[2]);
        list.append(listElement);
        index += 1;
      }
      target.append(list);
      continue;
    }

    if (/^\s*>\s?/.test(line)) {
      const quote = document.createElement("blockquote");
      const quoteLines = [];
      while (index < lines.length && /^\s*>\s?/.test(lines[index])) {
        quoteLines.push(lines[index].replace(/^\s*>\s?/, ""));
        index += 1;
      }
      inline(quote, quoteLines.join("\n"));
      target.append(quote);
      continue;
    }

    const paragraphLines = [line];
    index += 1;
    while (
      index < lines.length &&
      lines[index].trim() &&
      !startsBlock(lines[index])
    ) {
      paragraphLines.push(lines[index]);
      index += 1;
    }
    const paragraph = document.createElement("p");
    inline(paragraph, paragraphLines.join("\n"));
    target.append(paragraph);
  }
}
function addMessage(role, text, timestamp) {
  const article = document.createElement("article");
  const messageTimestamp = asDate(timestamp) ? timestamp : Math.floor(Date.now() / 1000);
  article.className = "message " + (role === "user" ? "user" : "assistant");
  const content = document.createElement("div");
  content.className = "content";
  if (role === "user") content.textContent = text;
  else markdown(content, text);
  article.append(content);
  if (role !== "user") {
    const meta = document.createElement("div");
    meta.className = "message-meta";
    const copy = document.createElement("button");
    copy.textContent = "复制";
    copy.onclick = async () => {
      const value = article.dataset.raw || "";
      try {
        await copyText(value);
        toast("已复制");
      } catch {
        toast("复制失败，请选择文本手动复制");
      }
    };
    meta.append(copy);
    const time = document.createElement("time");
    time.className = "message-time";
    time.textContent = formatTimestamp(messageTimestamp);
    const messageDate = asDate(messageTimestamp);
    if (messageDate) time.dateTime = messageDate.toISOString();
    meta.append(time);
    article.append(meta);
  }
  article.dataset.raw = text;
  $("messages").append(article);
  return { article, content };
}
function bottom() {
  $("scroll-area").scrollTop = $("scroll-area").scrollHeight;
}
function nearBottom() {
  const el = $("scroll-area");
  return el.scrollHeight - el.scrollTop - el.clientHeight < 100;
}

async function streamReply(id, message, onChunk) {
  const controller = new AbortController();
  let timer;
  const resetTimeout = () => {
    clearTimeout(timer);
    timer = setTimeout(() => controller.abort(), 180000);
  };
  let reader;
  try {
    resetTimeout();
    const response = await fetch(state.base + "/api/message/async", {
      method: "POST",
      signal: controller.signal,
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ session_id: id, message }),
    });
    if (!response.ok || !response.body)
      throw new Error((await response.text()).slice(0, 160) || "流式请求失败");
    reader = response.body.getReader();
    const decoder = new TextDecoder();
    let buffer = "",
      done = false;
    const event = (raw) => {
      const data = raw
        .split(/\r?\n/)
        .filter((line) => line.startsWith("data:"))
        .map((line) => line.slice(5).replace(/^ /, ""))
        .join("\n");
      if (!data) return;
      if (data.trim() === "[DONE]") {
        done = true;
        return;
      }
      let payload = JSON.parse(data);
      if (typeof payload === "string") payload = JSON.parse(payload);
      if (payload.success === false)
        throw new Error(payload.message || "生成失败");
      if (payload.data?.response) onChunk(payload.data.response);
    };
    while (!done) {
      const chunk = await reader.read();
      resetTimeout();
      buffer += decoder.decode(chunk.value || new Uint8Array(), {
        stream: !chunk.done,
      });
      let boundary;
      while (!done && (boundary = /\r?\n\r?\n/.exec(buffer))) {
        event(buffer.slice(0, boundary.index));
        buffer = buffer.slice(boundary.index + boundary[0].length);
      }
      if (chunk.done) {
        if (!done && buffer.trim()) event(buffer);
        break;
      }
    }
    if (!done)
      throw new Error("连接提前结束，回复可能不完整。请刷新会话确认。");
  } catch (error) {
    if (error.name === "AbortError")
      throw new Error("回复等待超时，请刷新会话确认");
    throw error;
  } finally {
    clearTimeout(timer);
    if (reader) {
      try {
        await reader.cancel();
      } catch {}
      reader.releaseLock();
    }
  }
}
async function sendMessage() {
  const message = $("message").value.trim();
  if (!message) return;
  if (!state.current) {
    await createSession();
    state.drafts.delete("");
  }
  const id = state.current;
  $("welcome").hidden = true;
  $("history-error").hidden = true;
  addMessage("user", message);
  $("message").value = "";
  state.drafts.delete(id);
  resize();
  const reply = addMessage("assistant", "");
  reply.article.classList.add("generating");
  bottom();
  let text = "",
    renderFrame = 0,
    followOutput = false;
  const renderReply = () => {
    renderFrame = 0;
    markdown(reply.content, text);
    reply.article.dataset.raw = text;
    if (followOutput) bottom();
    followOutput = false;
  };
  try {
    const append = (chunk) => {
      text += chunk;
      followOutput = followOutput || nearBottom();
      if (!renderFrame) renderFrame = requestAnimationFrame(renderReply);
    };
    if ($("stream").checked) await streamReply(id, message, append);
    else {
      const data = await api("/api/message", "POST", {
        session_id: id,
        message,
      });
      append(data?.response || "");
    }
    if (!text) throw new Error("服务器未返回回复，请刷新会话确认");
  } catch (error) {
    const errorNode = document.createElement("p");
    errorNode.className = "message-error";
    errorNode.textContent = error.message;
    reply.article.append(errorNode);
    if (!text) {
      $("message").value = message;
      resize();
    }
    toast(error.message);
  } finally {
    const follow = nearBottom();
    if (renderFrame) cancelAnimationFrame(renderFrame);
    reply.article.dataset.raw = text;
    reply.article.classList.remove("generating");
    markdown(reply.content, text);
    if (follow) bottom();
    // The backend owns first-message naming; never derive names from an AI reply.
    try {
      await refreshSessions();
    } catch {
      toast("回复已保留，会话列表刷新失败");
    }
  }
}
function resize() {
  $("message").style.height = "auto";
  $("message").style.height =
    Math.min(180, Math.max(32, $("message").scrollHeight)) + "px";
  $("count").textContent = $("message").value.length
    ? $("message").value.length + " / 12000"
    : "";
  controls();
}
function configPanel() {
  const cached = state.current
    ? state.configs.get(state.model)
    : state.draftConfig || state.configs.get(state.model);
  const config = cached || defaults;
  $("temperature").value = config.temperature;
  $("tokens").value = config.max_tokens;
  $("think").checked = config.think;
  document.querySelectorAll("[name=effort]").forEach((input) => {
    input.checked = input.value === config.reasoning_effort;
  });
  $("model-desc").textContent =
    state.models.find((m) => m.name === state.model)?.desc || "";
  $("config-note").textContent = cached
    ? "参数应用于同名模型。"
    : "当前显示建议值；点击应用后生效，影响同名模型。";
}
async function connect() {
  connection(false, "正在连接");
  try {
    const [models, sessions] = await Promise.all([
      api("/api/models"),
      api("/api/sessions"),
    ]);
    state.models = list(models);
    state.sessions = list(sessions);
    cacheSessionConfigs();
    $("model").replaceChildren(
      ...state.models.map((m) => new Option(m.name, m.name)),
    );
    if (!state.models.some((m) => m.name === state.model))
      state.model = state.models[0]?.name || "";
    connection(true, "已连接");
    updateHeader();
    renderSessions();
  } catch (error) {
    connection(false, "连接失败 · 点击重试");
    throw error;
  }
}

$("theme").onclick = () =>
  theme(document.documentElement.dataset.theme === "dark" ? "light" : "dark");
$("collapse").onclick = () => {
  if (matchMedia("(max-width:760px)").matches) closeSidebar();
  else document.body.classList.add("collapsed");
};
$("expand").onclick = () => {
  document.body.classList.remove("collapsed");
  if (matchMedia("(max-width:760px)").matches) {
    document.body.classList.add("mobile-open");
    $("backdrop").hidden = false;
  }
};
$("backdrop").onclick = closeSidebar;
$("new-chat").onclick = home;
$("name-new").onclick = () => openName();
$("rename-title").onclick = () => state.current && openName(current());
$("search").oninput = renderSessions;
$("refresh").onclick = () =>
  action("刷新中…", async () => {
    await connect();
    if (state.current) {
      if (current()) await selectSession(state.current);
      else home();
    }
  });
$("retry-history").onclick = () =>
  action("载入会话…", () => selectSession(state.current));
$("clear").onclick = () => confirmDelete(null);
$("name-form").onsubmit = (event) => {
  event.preventDefault();
  action("保存中…", async () => {
    const name = $("name").value.trim();
    if (!name) throw new Error("名称不能为空");
    if (state.rename) {
      const id = state.rename;
      await api("/api/session/name", "POST", {
        session_id: id,
        session_name: name,
      });
      const s = state.sessions.find((s) => s.session_id === id);
      if (s) s.session_name = name;
      updateHeader();
      renderSessions();
    } else {
      saveDraft();
      await createSession(name);
      $("message").value = "";
      resize();
      closeSidebar();
    }
    $("name-dialog").close();
  });
};
$("confirm-form").onsubmit = (event) => {
  event.preventDefault();
  action("删除中…", async () => {
    const id = state.deletion;
    await api(
      id === "*" ? "/api/sessions" : "/api/session/" + encodeURIComponent(id),
      "DELETE",
    );
    state.sessions =
      id === "*" ? [] : state.sessions.filter((s) => s.session_id !== id);
    if (id === "*") state.drafts.clear();
    else state.drafts.delete(id);
    if (id === "*" || id === state.current) {
      $("message").value = "";
      home();
    }
    renderSessions();
    $("confirm-dialog").close();
    toast(id === "*" ? "全部会话已清空" : "会话已删除");
  });
};
$("model-toggle").onclick = () => {
  const open = $("model-panel").hidden;
  $("model-panel").hidden = !open;
  $("model-toggle").setAttribute("aria-expanded", String(open));
  if (open) configPanel();
};
$("close-model").onclick = closePanel;
$("model").onchange = () => {
  const model = $("model").value,
    old = state.model;
  action("切换模型…", async () => {
    try {
      if (state.current) {
        await api("/api/session/model", "POST", {
          session_id: state.current,
          model,
        });
        current().model = model;
      }
      state.model = model;
      state.draftConfig = null;
      updateHeader();
      renderSessions();
      configPanel();
    } catch (error) {
      $("model").value = old;
      throw error;
    }
  });
};
$("parameters").onsubmit = (event) => {
  event.preventDefault();
  action("应用参数…", async () => {
    const config = {
      temperature: Number($("temperature").value),
      max_tokens: Number($("tokens").value),
      think: $("think").checked,
      reasoning_effort: document.querySelector("[name=effort]:checked").value,
    };
    if (
      !Number.isFinite(config.temperature) ||
      config.temperature < 0 ||
      config.temperature > 2 ||
      !Number.isInteger(config.max_tokens) ||
      config.max_tokens < 1 ||
      config.max_tokens > 2147483647
    )
      throw new Error("请检查参数范围");
    if (state.current) {
      await api("/api/session/model_config", "POST", {
        ...config,
        session_id: state.current,
        model: state.model,
      });
      state.configs.set(state.model, config);
    } else state.draftConfig = config;
    closePanel();
    toast(state.current ? "参数已应用" : "参数将在创建会话时应用");
  });
};
$("composer").onsubmit = (event) => {
  event.preventDefault();
  closePanel();
  action("正在回复…", sendMessage).then(() => {
    if (!state.busy) $("message").focus();
  });
};
$("message").oninput = resize;
$("message").onkeydown = (event) => {
  if (
    event.key === "Enter" &&
    !event.shiftKey &&
    !event.isComposing &&
    event.keyCode !== 229
  ) {
    event.preventDefault();
    $("composer").requestSubmit();
  }
};
$("connection").onclick = () => {
  $("api-base").value = state.base;
  $("connection-dialog").showModal();
};
$("connection-form").onsubmit = (event) => {
  event.preventDefault();
  action("连接中…", async () => {
    const base = $("api-base").value.trim().replace(/\/$/, "");
    if (base && !/^https?:\/\//i.test(base))
      throw new Error("地址必须以 http:// 或 https:// 开头");
    state.base = base;
    storage.set("chatserver-api-base", base);
    state.models = [];
    state.model = "";
    $("model").replaceChildren();
    state.sessions = [];
    state.configs.clear();
    state.drafts.clear();
    state.draftConfig = null;
    $("message").value = "";
    home();
    await connect();
    $("connection-dialog").close();
  });
};
document.querySelectorAll("[data-close]").forEach((button) => {
  button.onclick = () => button.closest("dialog").close();
});
document.querySelectorAll("dialog").forEach((dialog) => {
  dialog.addEventListener("cancel", (event) => {
    if (state.busy) event.preventDefault();
  });
});
document.addEventListener("click", (event) => {
  if (
    !state.busy &&
    !$("model-panel").contains(event.target) &&
    !$("model-toggle").contains(event.target)
  )
    closePanel();
});
document.addEventListener("keydown", (event) => {
  if (event.key === "Escape") {
    if (!state.busy) closePanel();
    closeSidebar();
  }
  if ((event.ctrlKey || event.metaKey) && event.key.toLowerCase() === "k") {
    event.preventDefault();
    if (!state.busy && !document.querySelector("dialog[open]")) {
      home();
      $("message").focus();
    }
  }
});
theme(storage.get("chatserver-appearance") === "dark" ? "dark" : "light");
resize();
action("连接中…", connect);
