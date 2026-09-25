"use strict";

const elements = {
  sessionList: document.querySelector("#session-list"),
  newSessionButton: document.querySelector("#new-session-button"),
  emptyNewSessionButton: document.querySelector("#empty-new-session-button"),
  refreshSessionsButton: document.querySelector("#refresh-sessions-button"),
  clearSessionsButton: document.querySelector("#clear-sessions-button"),
  deleteSessionButton: document.querySelector("#delete-session-button"),
  renameSessionButton: document.querySelector("#rename-session-button"),
  currentSessionTitle: document.querySelector("#current-session-title"),
  modelSelect: document.querySelector("#model-select"),
  settingsButton: document.querySelector("#settings-button"),
  emptyState: document.querySelector("#empty-state"),
  chatView: document.querySelector("#chat-view"),
  messageList: document.querySelector("#message-list"),
  messageForm: document.querySelector("#message-form"),
  messageInput: document.querySelector("#message-input"),
  sendButton: document.querySelector("#send-button"),
  streamToggle: document.querySelector("#stream-toggle"),
  characterCount: document.querySelector("#character-count"),
  apiBaseInput: document.querySelector("#api-base-input"),
  applyApiButton: document.querySelector("#apply-api-button"),
  connectionDot: document.querySelector("#connection-dot"),
  connectionLabel: document.querySelector("#connection-label"),
  newSessionDialog: document.querySelector("#new-session-dialog"),
  newSessionForm: document.querySelector("#new-session-form"),
  newSessionName: document.querySelector("#new-session-name"),
  newSessionModel: document.querySelector("#new-session-model"),
  newModelDescription: document.querySelector("#new-model-description"),
  settingsDialog: document.querySelector("#settings-dialog"),
  settingsForm: document.querySelector("#settings-form"),
  renameSessionDialog: document.querySelector("#rename-session-dialog"),
  renameSessionForm: document.querySelector("#rename-session-form"),
  sessionNameInput: document.querySelector("#session-name-input"),
  temperatureInput: document.querySelector("#temperature-input"),
  maxTokensInput: document.querySelector("#max-tokens-input"),
  reasoningEffortSelect: document.querySelector("#reasoning-effort-select"),
  thinkInput: document.querySelector("#think-input"),
  toast: document.querySelector("#toast")
};

const state = {
  apiBase: localStorage.getItem("chatserver-api-base") || "",
  models: [],
  sessions: [],
  currentSessionId: "",
  currentModel: "",
  sending: false
};

let toastTimer;

function apiUrl(path) {
  return `${state.apiBase.replace(/\/$/, "")}${path}`;
}

function showToast(message, type = "info") {
  clearTimeout(toastTimer);
  elements.toast.textContent = message;
  elements.toast.className = `toast show${type === "error" ? " error" : ""}`;
  toastTimer = setTimeout(() => { elements.toast.className = "toast"; }, 3200);
}

function setConnection(online, label) {
  elements.connectionDot.className = `status-dot ${online ? "online" : "offline"}`;
  elements.connectionLabel.textContent = label;
}

async function request(path, options = {}) {
  const response = await fetch(apiUrl(path), {
    ...options,
    headers: {
      "Content-Type": "application/json",
      ...(options.headers || {})
    }
  });
  const text = await response.text();
  let payload;
  try {
    payload = text ? JSON.parse(text) : {};
  } catch {
    payload = { success: false, message: text || `HTTP ${response.status}` };
  }
  if(!response.ok || payload.success === false) {
    throw new Error(payload.message || `请求失败：HTTP ${response.status}`);
  }
  return payload;
}

function normalizeList(value) {
  return Array.isArray(value) ? value : [];
}

function formatTime(timestamp) {
  if(!timestamp) return "";
  return new Intl.DateTimeFormat("zh-CN", {
    month: "2-digit", day: "2-digit", hour: "2-digit", minute: "2-digit"
  }).format(new Date(timestamp * 1000));
}

function sessionTitle(session) {
  const source = session?.session_name || session?.first_message;
  if(!source || source === "new session") return "新会话";
  const normalized = source.replace(/\s+/g, " ").trim();
  const characters = Array.from(normalized);
  return characters.length > 20 ? `${characters.slice(0, 20).join("")}…` : normalized;
}

function renderModels() {
  const options = state.models.map(model => {
    const option = document.createElement("option");
    option.value = model.name;
    option.textContent = model.name;
    return option;
  });
  elements.modelSelect.replaceChildren(...options.map(option => option.cloneNode(true)));
  elements.newSessionModel.replaceChildren(...options);
  elements.modelSelect.disabled = !state.currentSessionId || state.models.length === 0;
  if(state.currentModel) elements.modelSelect.value = state.currentModel;
  updateNewModelDescription();
}

function renderSessions() {
  elements.clearSessionsButton.disabled = state.sessions.length === 0;
  if(state.sessions.length === 0) {
    const empty = document.createElement("div");
    empty.className = "session-empty";
    empty.textContent = "暂无会话，点击上方按钮创建。";
    elements.sessionList.replaceChildren(empty);
    return;
  }
  const items = state.sessions.map(session => {
    const button = document.createElement("button");
    button.type = "button";
    button.className = `session-item${session.session_id === state.currentSessionId ? " active" : ""}`;
    button.dataset.sessionId = session.session_id;

    const title = document.createElement("span");
    title.className = "session-item-title";
    title.textContent = sessionTitle(session);
    const meta = document.createElement("span");
    meta.className = "session-item-meta";
    const model = document.createElement("span");
    model.textContent = session.model || "未知模型";
    const count = document.createElement("span");
    count.textContent = `${session.message_count || 0} 条`;
    meta.append(model, count);
    button.append(title, meta);
    button.addEventListener("click", () => selectSession(session.session_id));
    return button;
  });
  elements.sessionList.replaceChildren(...items);
}

function setActiveSession(session) {
  state.currentSessionId = session?.session_id || "";
  state.currentModel = session?.model || "";
  elements.currentSessionTitle.textContent = session ? sessionTitle(session) : "尚未选择会话";
  elements.deleteSessionButton.disabled = !state.currentSessionId;
  elements.renameSessionButton.disabled = !state.currentSessionId;
  elements.settingsButton.disabled = !state.currentSessionId;
  elements.modelSelect.disabled = !state.currentSessionId;
  elements.emptyState.classList.toggle("hidden", Boolean(state.currentSessionId));
  elements.chatView.classList.toggle("hidden", !state.currentSessionId);
  if(state.currentModel) elements.modelSelect.value = state.currentModel;
  renderSessions();
}

function appendMessage(role, content, timestamp = Math.floor(Date.now() / 1000), streaming = false) {
  const article = document.createElement("article");
  article.className = `message ${role === "user" ? "user" : "assistant"}`;
  const avatar = document.createElement("div");
  avatar.className = "avatar";
  avatar.textContent = role === "user" ? "YOU" : "AI";
  const body = document.createElement("div");
  body.className = "message-body";
  const text = document.createElement("pre");
  text.className = `message-content${streaming ? " typing" : ""}`;
  text.textContent = content;
  const time = document.createElement("span");
  time.className = "message-time";
  time.textContent = formatTime(timestamp);
  body.append(text, time);
  article.append(avatar, body);
  elements.messageList.append(article);
  elements.messageList.scrollTop = elements.messageList.scrollHeight;
  return text;
}

function renderHistory(messages) {
  elements.messageList.replaceChildren();
  const ordered = [...normalizeList(messages)].sort((a, b) => (a.timestamp || 0) - (b.timestamp || 0));
  if(ordered.length === 0) {
    const placeholder = appendMessage("assistant", "会话已创建。输入一条消息开始测试接口。", undefined, false);
    placeholder.closest(".message").classList.add("welcome-message");
    return;
  }
  ordered.forEach(message => appendMessage(message.role, message.content, message.timestamp));
}

async function loadModels() {
  const payload = await request("/api/models");
  state.models = normalizeList(payload.data);
  renderModels();
}

async function loadSessions(selectFirst = false) {
  const payload = await request("/api/sessions");
  state.sessions = normalizeList(payload.data);
  renderSessions();
  if(state.currentSessionId) {
    const current = state.sessions.find(item => item.session_id === state.currentSessionId);
    if(current) elements.currentSessionTitle.textContent = sessionTitle(current);
  }
  if(selectFirst && !state.currentSessionId && state.sessions.length > 0) {
    await selectSession(state.sessions[0].session_id);
  }
}

async function selectSession(sessionId) {
  const session = state.sessions.find(item => item.session_id === sessionId);
  if(!session) return;
  setActiveSession(session);
  try {
    const payload = await request(`/api/session/${encodeURIComponent(sessionId)}/history`);
    renderHistory(payload.data);
  } catch(error) {
    showToast(error.message, "error");
  }
}

function openNewSessionDialog() {
  if(state.models.length === 0) {
    showToast("当前没有可用模型", "error");
    return;
  }
  elements.newSessionName.value = "new session";
  updateNewModelDescription();
  elements.newSessionDialog.showModal();
}

function updateNewModelDescription() {
  const selected = state.models.find(model => model.name === elements.newSessionModel.value);
  elements.newModelDescription.textContent = selected?.desc || "暂无模型描述";
}

async function createSession(event) {
  event.preventDefault();
  const model = elements.newSessionModel.value;
  const sessionName = elements.newSessionName.value.replace(/\s+/g, " ").trim();
  if(!sessionName) {
    showToast("会话名称不能为空", "error");
    return;
  }
  try {
    const payload = await request("/api/session", {
      method: "POST",
      body: JSON.stringify({ model, session_name: sessionName })
    });
    elements.newSessionDialog.close();
    await loadSessions();
    const created = state.sessions.find(item => item.session_id === payload.data.session_id) || {
      session_id: payload.data.session_id,
      session_name: payload.data.session_name,
      model
    };
    setActiveSession(created);
    renderHistory([]);
    showToast("会话创建成功");
  } catch(error) {
    showToast(error.message, "error");
  }
}

async function deleteCurrentSession() {
  if(!state.currentSessionId) return;
  if(!window.confirm("确认删除当前会话及其历史消息吗？")) return;
  try {
    await request(`/api/session/${encodeURIComponent(state.currentSessionId)}`, { method: "DELETE" });
    state.currentSessionId = "";
    state.currentModel = "";
    setActiveSession(null);
    elements.messageList.replaceChildren();
    await loadSessions(true);
    showToast("会话已删除");
  } catch(error) {
    showToast(error.message, "error");
  }
}

async function deleteAllSessions() {
  if(state.sessions.length === 0) return;
  if(!window.confirm("确认清空全部会话及其历史消息吗？此操作无法撤销。")) return;
  try {
    await request("/api/sessions", { method: "DELETE" });
    state.sessions = [];
    state.currentSessionId = "";
    state.currentModel = "";
    elements.messageList.replaceChildren();
    setActiveSession(null);
    showToast("全部会话已清空");
  } catch(error) {
    showToast(error.message, "error");
  }
}

async function changeModel() {
  if(!state.currentSessionId || !elements.modelSelect.value) return;
  const previous = state.currentModel;
  const model = elements.modelSelect.value;
  try {
    await request("/api/session/model", {
      method: "POST",
      body: JSON.stringify({ session_id: state.currentSessionId, model })
    });
    state.currentModel = model;
    const session = state.sessions.find(item => item.session_id === state.currentSessionId);
    if(session) session.model = model;
    renderSessions();
    showToast(`模型已切换为 ${model}`);
  } catch(error) {
    elements.modelSelect.value = previous;
    showToast(error.message, "error");
  }
}

function openRenameSessionDialog() {
  const session = state.sessions.find(item => item.session_id === state.currentSessionId);
  if(!session) return;
  const currentName = session.session_name === "new session" ? "" : (session.session_name || "");
  elements.sessionNameInput.value = Array.from(currentName).slice(0, 20).join("");
  elements.renameSessionDialog.showModal();
  elements.sessionNameInput.focus();
  elements.sessionNameInput.select();
}

async function renameSession(event) {
  event.preventDefault();
  const sessionName = elements.sessionNameInput.value.replace(/\s+/g, " ").trim();
  if(!sessionName) {
    showToast("会话名称不能为空", "error");
    return;
  }
  try {
    await request("/api/session/name", {
      method: "POST",
      body: JSON.stringify({
        session_id: state.currentSessionId,
        session_name: sessionName
      })
    });
    const session = state.sessions.find(item => item.session_id === state.currentSessionId);
    if(session) session.session_name = sessionName;
    elements.currentSessionTitle.textContent = sessionName;
    renderSessions();
    elements.renameSessionDialog.close();
    showToast("会话名称已更新");
  } catch(error) {
    showToast(error.message, "error");
  }
}

async function saveModelSettings(event) {
  event.preventDefault();
  try {
    await request("/api/session/model_config", {
      method: "POST",
      body: JSON.stringify({
        session_id: state.currentSessionId,
        model: state.currentModel,
        temperature: Number(elements.temperatureInput.value),
        max_tokens: Number(elements.maxTokensInput.value),
        think: elements.thinkInput.checked,
        reasoning_effort: elements.reasoningEffortSelect.value
      })
    });
    elements.settingsDialog.close();
    showToast("模型参数已更新");
  } catch(error) {
    showToast(error.message, "error");
  }
}

function parseSsePayload(raw) {
  let payload = JSON.parse(raw);
  if(typeof payload === "string") payload = JSON.parse(payload);
  return payload;
}

async function sendStreamingMessage(message, responseElement) {
  const response = await fetch(apiUrl("/api/message/async"), {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ session_id: state.currentSessionId, message })
  });
  if(!response.ok || !response.body) {
    const errorText = await response.text();
    throw new Error(errorText || `流式请求失败：HTTP ${response.status}`);
  }

  const reader = response.body.getReader();
  const decoder = new TextDecoder();
  let buffer = "";
  let done = false;

  while(!done) {
    const result = await reader.read();
    done = result.done;
    buffer += decoder.decode(result.value || new Uint8Array(), { stream: !done });
    const events = buffer.split(/\r?\n\r?\n/);
    buffer = events.pop() || "";
    for(const event of events) {
      const data = event.split(/\r?\n/)
        .filter(line => line.startsWith("data:"))
        .map(line => line.slice(5).trimStart())
        .join("\n");
      if(!data) continue;
      if(data.trim() === "[DONE]") {
        done = true;
        break;
      }
      const payload = parseSsePayload(data);
      if(payload.success === false) throw new Error(payload.message || "流式响应失败");
      responseElement.textContent += payload.data?.response || "";
      elements.messageList.scrollTop = elements.messageList.scrollHeight;
    }
  }
}

async function sendMessage(event) {
  event.preventDefault();
  const message = elements.messageInput.value.trim();
  if(!message || !state.currentSessionId || state.sending) return;

  state.sending = true;
  elements.sendButton.disabled = true;
  elements.messageInput.disabled = true;
  const currentSession = state.sessions.find(item => item.session_id === state.currentSessionId);
  if(currentSession && (!currentSession.session_name || currentSession.session_name === "new session")) {
    currentSession.session_name = Array.from(message.replace(/\s+/g, " ").trim()).slice(0, 20).join("");
    elements.currentSessionTitle.textContent = sessionTitle(currentSession);
    renderSessions();
  }
  elements.messageList.querySelector(".welcome-message")?.remove();
  appendMessage("user", message);
  elements.messageInput.value = "";
  updateComposer();
  const responseElement = appendMessage("assistant", "", undefined, true);

  try {
    if(elements.streamToggle.checked) {
      await sendStreamingMessage(message, responseElement);
    } else {
      const payload = await request("/api/message", {
        method: "POST",
        body: JSON.stringify({ session_id: state.currentSessionId, message })
      });
      responseElement.textContent = payload.data?.response || "";
    }
    responseElement.classList.remove("typing");
    await loadSessions();
  } catch(error) {
    responseElement.classList.remove("typing");
    responseElement.textContent = `请求失败：${error.message}`;
    showToast(error.message, "error");
  } finally {
    state.sending = false;
    elements.sendButton.disabled = false;
    elements.messageInput.disabled = false;
    elements.messageInput.focus();
  }
}

function updateComposer() {
  elements.characterCount.textContent = `${elements.messageInput.value.length} / 12000`;
  elements.messageInput.style.height = "auto";
  elements.messageInput.style.height = `${Math.min(elements.messageInput.scrollHeight, 180)}px`;
}

async function connect() {
  setConnection(false, "正在连接…");
  try {
    await loadModels();
    await loadSessions(true);
    setConnection(true, "服务正常");
  } catch(error) {
    setConnection(false, "连接失败");
    showToast(error.message, "error");
  }
}

elements.apiBaseInput.value = state.apiBase;
elements.newSessionButton.addEventListener("click", openNewSessionDialog);
elements.emptyNewSessionButton.addEventListener("click", openNewSessionDialog);
elements.refreshSessionsButton.addEventListener("click", () => loadSessions().catch(error => showToast(error.message, "error")));
elements.clearSessionsButton.addEventListener("click", deleteAllSessions);
elements.newSessionModel.addEventListener("change", updateNewModelDescription);
elements.newSessionForm.addEventListener("submit", createSession);
elements.deleteSessionButton.addEventListener("click", deleteCurrentSession);
elements.renameSessionButton.addEventListener("click", openRenameSessionDialog);
elements.modelSelect.addEventListener("change", changeModel);
elements.settingsButton.addEventListener("click", () => elements.settingsDialog.showModal());
elements.settingsForm.addEventListener("submit", saveModelSettings);
elements.renameSessionForm.addEventListener("submit", renameSession);
elements.messageForm.addEventListener("submit", sendMessage);
elements.messageInput.addEventListener("input", updateComposer);
elements.messageInput.addEventListener("keydown", event => {
  if(event.key === "Enter" && !event.shiftKey) {
    event.preventDefault();
    elements.messageForm.requestSubmit();
  }
});
elements.applyApiButton.addEventListener("click", () => {
  state.apiBase = elements.apiBaseInput.value.trim().replace(/\/$/, "");
  localStorage.setItem("chatserver-api-base", state.apiBase);
  state.currentSessionId = "";
  setActiveSession(null);
  connect();
});
document.querySelectorAll("[data-close-dialog]").forEach(button => {
  button.addEventListener("click", () => button.closest("dialog").close());
});

connect();
