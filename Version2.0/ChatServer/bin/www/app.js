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
  eye: '<path d="M2 12s3.5-6 10-6 10 6 10 6-3.5 6-10 6S2 12 2 12Z"/><circle cx="12" cy="12" r="2.5"/>',
  eyeOff:
    '<path d="m3 3 18 18M10.6 6.2A11 11 0 0 1 12 6c6.5 0 10 6 10 6a17 17 0 0 1-3 3.7M6.2 6.2C3.4 8 2 12 2 12s3.5 6 10 6a11 11 0 0 0 3.1-.4M9.9 9.9a3 3 0 0 0 4.2 4.2"/>',
  spark:
    '<path d="M12 3c2-2 5 0 5 2 4-1 6 3 4 6 3 3 0 7-3 7 0 4-5 5-7 2-4 2-7-1-6-4-4-2-3-6 0-8-1-3 4-6 7-5Z"/><path d="m8 10 2 2-2 2m5 1h3"/>',
  bolt: '<path d="m13 2-8 12h6l-1 8 9-13h-6Z"/>',
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
  user: null,
  authMode: "login",
  models: [],
  sessions: [],
  current: "",
  model: "",
  busy: false,
  loading: false,
  online: false,
  configs: new Map(),
  modelConfigs: new Map(),
  draftConfig: null,
  drafts: new Map(),
  menu: "",
  rename: null,
  deletion: null,
  avatarRevision: 0,
  pendingAvatarFile: null,
  avatarPreviewUrl: "",
  verifiedEmails: new Map(),
  pendingVerification: null,
};
const codeTimers = new Map();
const reducedMotion = matchMedia("(prefers-reduced-motion: reduce)");
const dialogClosures = new WeakMap();
const panelClosures = new WeakMap();
let advancedAnimation = null;
const defaultSidebarWidth = 258;
const minimumSidebarWidth = 220;
const maximumSidebarWidth = 420;
const maximumAvatarBytes = 5 * 1024 * 1024;
const supportedAvatarTypes = new Set(["image/png", "image/jpeg"]);
class ApiError extends Error {
  constructor(message, status = 0, data = null) {
    super(message);
    this.name = "ApiError";
    this.status = status;
    this.data = data;
  }
}
const defaults = {
  temperature: 0.8,
  max_tokens: 393216,
  think: true,
  reasoning_effort: "high",
};
function normalizeModelConfig(config) {
  if (!config || typeof config !== "object") return null;
  const temperature = Number(config.temperature);
  const maxTokens = Number(config.max_tokens);
  const reasoningEffort = ["low", "high", "max"].includes(
    config.reasoning_effort,
  )
    ? config.reasoning_effort
    : defaults.reasoning_effort;
  return {
    temperature: Number.isFinite(temperature)
      ? temperature
      : defaults.temperature,
    max_tokens:
      Number.isInteger(maxTokens) && maxTokens > 0
        ? maxTokens
        : defaults.max_tokens,
    think: typeof config.think === "boolean" ? config.think : defaults.think,
    reasoning_effort: reasoningEffort,
  };
}
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
function cancelDialogClosure(dialog) {
  const closure = dialogClosures.get(dialog);
  if (!closure) return;
  clearTimeout(closure.fallback);
  dialog.removeEventListener("transitionend", closure.onTransitionEnd);
  dialogClosures.delete(dialog);
}
function closeDialogImmediately(dialog) {
  cancelDialogClosure(dialog);
  if (dialog.open) dialog.close();
  dialog.classList.remove("dialog-visible", "dialog-closing");
}
function openDialog(dialogOrId) {
  const dialog =
    typeof dialogOrId === "string" ? $(dialogOrId) : dialogOrId;
  if (!dialog || dialog.open) return;
  cancelDialogClosure(dialog);
  dialog.classList.remove("dialog-visible", "dialog-closing");
  dialog.showModal();
  // 强制浏览器提交初始透明状态，再启动进入过渡。
  void dialog.offsetWidth;
  dialog.classList.add("dialog-visible");
}
function closeDialog(dialogOrId, afterClose) {
  const dialog =
    typeof dialogOrId === "string" ? $(dialogOrId) : dialogOrId;
  if (!dialog?.open) {
    afterClose?.();
    return;
  }
  if (dialog.classList.contains("dialog-closing")) return;

  let fallback;
  const finish = () => {
    const closure = dialogClosures.get(dialog);
    if (!closure || closure.finish !== finish) return;
    cancelDialogClosure(dialog);
    if (dialog.open) dialog.close();
    dialog.classList.remove("dialog-visible", "dialog-closing");
    afterClose?.();
  };
  const onTransitionEnd = (event) => {
    if (event.target === dialog && event.propertyName === "opacity") finish();
  };

  if (reducedMotion.matches) {
    dialog.close();
    dialog.classList.remove("dialog-visible", "dialog-closing");
    afterClose?.();
    return;
  }

  dialog.classList.remove("dialog-visible");
  dialog.classList.add("dialog-closing");
  dialog.addEventListener("transitionend", onTransitionEnd);
  fallback = setTimeout(finish, 260);
  dialogClosures.set(dialog, { fallback, finish, onTransitionEnd });
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
    "model-toggle",
    "send",
    "retry-history",
    "connection",
    "stream",
    "user-menu",
  ])
    $(id).disabled = state.busy;
  document
    .querySelectorAll(
      "#sessions button, dialog button, dialog input, .composer-popover button, .composer-popover input, #auth-view button, #auth-view input",
    )
    .forEach((el) => {
      el.disabled = state.busy || el.dataset.cooldown === "true";
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
async function api(path, method = "GET", body, options = {}) {
  const controller = new AbortController();
  const timer = setTimeout(
    () => controller.abort(),
    path === "/api/message" ? 180000 : 30000,
  );
  try {
    const response = await fetch(state.base + path, {
      method,
      signal: controller.signal,
      credentials: "include",
      headers: body === undefined ? {} : { "Content-Type": "application/json" },
      body: body === undefined ? undefined : JSON.stringify(body),
    });
    const raw = await response.text();
    let data;
    try {
      data = JSON.parse(raw);
    } catch {
      data = { success: response.ok, message: raw.slice(0, 160) };
    }
    if (!response.ok || data.success === false) {
      const error = new ApiError(
        data.message || "请求失败：" + response.status,
        response.status,
        data.data,
      );
      if (response.status === 401 && !options.allowUnauthorized)
        showAuth("login", "登录状态已失效，请重新登录");
      throw error;
    }
    return data.data;
  } catch (error) {
    if (error.name === "AbortError")
      throw new Error("请求超时，请刷新确认服务器状态后再试");
    throw error;
  } finally {
    clearTimeout(timer);
  }
}
async function uploadAvatar(file) {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), 30000);
  try {
    const response = await fetch(state.base + "/api/user/avatar", {
      method: "POST",
      signal: controller.signal,
      credentials: "include",
      headers: { "Content-Type": file.type },
      // 该接口接收图片原始二进制，不能使用 JSON 或 multipart/form-data 包装。
      body: file,
    });
    const raw = await response.text();
    let data;
    try {
      data = JSON.parse(raw);
    } catch {
      data = { success: response.ok, message: raw.slice(0, 160) };
    }
    if (!response.ok || data.success === false) {
      if (response.status === 401)
        showAuth("login", "登录状态已失效，请重新登录");
      throw new ApiError(
        data.message || "头像上传失败：" + response.status,
        response.status,
        data.data,
      );
    }
  } catch (error) {
    if (error.name === "AbortError") throw new Error("头像上传超时，请稍后重试");
    throw error;
  } finally {
    clearTimeout(timer);
  }
}
function clearPendingAvatar() {
  if (state.avatarPreviewUrl) URL.revokeObjectURL(state.avatarPreviewUrl);
  state.pendingAvatarFile = null;
  state.avatarPreviewUrl = "";
  $("avatar-preview").removeAttribute("src");
  $("avatar-confirm-name").textContent = "";
  $("avatar-confirm-size").textContent = "";
}
function avatarFileSize(bytes) {
  if (bytes < 1024 * 1024) return `${Math.ceil(bytes / 1024)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}
function openAvatarConfirmation(file) {
  clearPendingAvatar();
  state.pendingAvatarFile = file;
  state.avatarPreviewUrl = URL.createObjectURL(file);
  $("avatar-preview").src = state.avatarPreviewUrl;
  $("avatar-confirm-name").textContent = file.name || "新头像";
  $("avatar-confirm-size").textContent = avatarFileSize(file.size);
  openDialog("avatar-confirm-dialog");
}
function theme(value) {
  document.documentElement.dataset.theme = value;
  storage.set("chatserver-appearance", value);
  const themeIcon = icon(value === "dark" ? "sun" : "moon");
  const label = value === "dark" ? "切换亮色主题" : "切换暗色主题";
  for (const id of ["theme", "auth-theme"]) {
    $(id).innerHTML = themeIcon;
    $(id).title = label;
    $(id).setAttribute("aria-label", label);
  }
}
function setPasswordVisibility(input, button, visible) {
  input.type = visible ? "text" : "password";
  button.innerHTML = icon(visible ? "eyeOff" : "eye");
  button.setAttribute("aria-pressed", String(visible));
  button.setAttribute("aria-label", visible ? "隐藏密码" : "显示密码");
  button.title = visible ? "隐藏密码" : "显示密码";
}
function hideAllPasswords(root = document) {
  root.querySelectorAll(".password-toggle").forEach((button) => {
    const input = button.parentElement.querySelector("input");
    if (input) setPasswordVisibility(input, button, false);
  });
}
function initializePasswordToggles() {
  document.querySelectorAll('input[type="password"]').forEach((input) => {
    const wrapper = document.createElement("span");
    wrapper.className = "password-input";
    input.before(wrapper);
    wrapper.append(input);

    const button = document.createElement("button");
    button.type = "button";
    button.className = "password-toggle";
    setPasswordVisibility(input, button, false);
    button.addEventListener("click", () => {
      setPasswordVisibility(input, button, input.type === "password");
      input.focus({ preventScroll: true });
    });
    wrapper.append(button);

    const form = input.closest("form");
    if (form && !form.dataset.passwordResetBound) {
      form.dataset.passwordResetBound = "true";
      form.addEventListener("reset", () => hideAllPasswords(form));
    }
  });
}
function closeSidebar() {
  document.body.classList.remove("mobile-open");
  $("backdrop").hidden = true;
}
function sidebarWidthLimit() {
  return Math.max(
    minimumSidebarWidth,
    Math.min(maximumSidebarWidth, Math.floor(window.innerWidth * 0.45)),
  );
}
function setSidebarWidth(width, persist = false) {
  const maximum = sidebarWidthLimit();
  const next = Math.min(maximum, Math.max(minimumSidebarWidth, Number(width)));
  document.documentElement.style.setProperty("--sidebar-width", `${next}px`);
  $("sidebar-resizer").setAttribute("aria-valuemax", String(maximum));
  $("sidebar-resizer").setAttribute("aria-valuenow", String(next));
  if (persist) storage.set("chatai-sidebar-width", String(next));
  positionOpenPanel();
}
function initializeSidebarResizer() {
  const resizer = $("sidebar-resizer");
  const savedWidth = Number(storage.get("chatai-sidebar-width"));
  setSidebarWidth(
    Number.isFinite(savedWidth) && savedWidth > 0
      ? savedWidth
      : defaultSidebarWidth,
  );

  const finishResize = (event) => {
    if (!document.body.classList.contains("resizing-sidebar")) return;
    document.body.classList.remove("resizing-sidebar");
    if (resizer.hasPointerCapture?.(event.pointerId))
      resizer.releasePointerCapture(event.pointerId);
    storage.set(
      "chatai-sidebar-width",
      String(parseInt(resizer.getAttribute("aria-valuenow"), 10)),
    );
  };

  resizer.addEventListener("pointerdown", (event) => {
    if (matchMedia("(max-width:760px)").matches || event.button !== 0) return;
    event.preventDefault();
    document.body.classList.add("resizing-sidebar");
    resizer.setPointerCapture(event.pointerId);
    setSidebarWidth(event.clientX);
  });
  resizer.addEventListener("pointermove", (event) => {
    if (!document.body.classList.contains("resizing-sidebar")) return;
    setSidebarWidth(event.clientX);
  });
  resizer.addEventListener("pointerup", finishResize);
  resizer.addEventListener("pointercancel", finishResize);
  resizer.addEventListener("dblclick", () => {
    setSidebarWidth(defaultSidebarWidth, true);
  });
  resizer.addEventListener("keydown", (event) => {
    if (event.key !== "ArrowLeft" && event.key !== "ArrowRight") return;
    event.preventDefault();
    const current = Number(resizer.getAttribute("aria-valuenow"));
    setSidebarWidth(current + (event.key === "ArrowLeft" ? -10 : 10), true);
  });
  window.addEventListener("resize", () => {
    if (matchMedia("(max-width:760px)").matches) return;
    const storedWidth = Number(storage.get("chatai-sidebar-width"));
    setSidebarWidth(
      Number.isFinite(storedWidth) && storedWidth > 0
        ? storedWidth
        : Number(resizer.getAttribute("aria-valuenow")),
    );
  });
}
function cancelPanelClosure(panel) {
  const closure = panelClosures.get(panel);
  if (!closure) return;
  clearTimeout(closure.fallback);
  panel.removeEventListener("transitionend", closure.onTransitionEnd);
  panelClosures.delete(panel);
}
function hidePanel(panel, immediate = false) {
  cancelPanelClosure(panel);
  if (panel.hidden) return;

  const finish = () => {
    const closure = panelClosures.get(panel);
    if (!immediate && (!closure || closure.finish !== finish)) return;
    cancelPanelClosure(panel);
    panel.hidden = true;
    panel.classList.remove("popover-visible", "popover-closing");
  };

  if (immediate || reducedMotion.matches) {
    panel.hidden = true;
    panel.classList.remove("popover-visible", "popover-closing");
    return;
  }

  const onTransitionEnd = (event) => {
    if (event.target === panel && event.propertyName === "opacity") finish();
  };
  panel.classList.remove("popover-visible");
  panel.classList.add("popover-closing");
  panel.addEventListener("transitionend", onTransitionEnd);
  const fallback = setTimeout(finish, 240);
  panelClosures.set(panel, { fallback, finish, onTransitionEnd });
}
function closePanel(immediate = false) {
  hidePanel($("model-panel"), immediate);
  hidePanel($("model-list-panel"), immediate);
  $("model-toggle").setAttribute("aria-expanded", "false");
  $("model-picker").setAttribute("aria-expanded", "false");
}
function effectiveConfig() {
  if (state.current)
    return (
      state.configs.get(state.model) ||
      state.modelConfigs.get(state.model) ||
      defaults
    );
  return (
    state.draftConfig || state.modelConfigs.get(state.model) || defaults
  );
}
function effortLabel(value) {
  return { low: "低", high: "中", max: "高" }[value] || "中";
}
function updateModelControl(config = effectiveConfig()) {
  const name = state.model || "选择模型";
  $("model-label").textContent = name;
  $("panel-model-label").textContent = name;
  $("effort-label").textContent = config.think
    ? effortLabel(config.reasoning_effort)
    : "关闭";
}
function syncThinkToggle() {
  const enabled = $("think").checked;
  const toggle = $("think-toggle");
  toggle.setAttribute("aria-pressed", String(enabled));
  toggle.setAttribute("aria-label", enabled ? "关闭思考模式" : "开启思考模式");
  toggle.title = enabled ? "思考模式已开启" : "思考模式已关闭";
  $("parameters")
    .querySelector(".effort-fieldset")
    .classList.toggle("thinking-disabled", !enabled);
}
function setAdvancedExpanded(expanded, animate = true) {
  const toggle = $("advanced-toggle");
  const content = $("advanced-content");
  const wasHidden = content.hidden;
  const currentHeight = wasHidden ? 0 : content.getBoundingClientRect().height;
  const currentStyle = wasHidden ? null : getComputedStyle(content);
  const startOpacity = currentStyle ? Number(currentStyle.opacity) : 0;
  const startTransform =
    currentStyle && currentStyle.transform !== "none"
      ? currentStyle.transform
      : expanded
        ? "translateY(-4px)"
        : "translateY(0)";

  advancedAnimation?.cancel();
  advancedAnimation = null;
  content.hidden = false;
  const targetHeight = expanded ? content.scrollHeight : 0;
  toggle.setAttribute("aria-expanded", String(expanded));
  content.classList.toggle("is-open", expanded);

  if (!animate || reducedMotion.matches || typeof content.animate !== "function") {
    content.hidden = !expanded;
    positionOpenPanel();
    return;
  }

  const animation = content.animate(
    [
      {
        height: `${currentHeight}px`,
        opacity: startOpacity,
        transform: startTransform,
      },
      {
        height: `${targetHeight}px`,
        opacity: expanded ? 1 : 0,
        transform: expanded ? "translateY(0)" : "translateY(-4px)",
      },
    ],
    {
      duration: 240,
      easing: "cubic-bezier(0.22, 1, 0.36, 1)",
      fill: "both",
    },
  );
  advancedAnimation = animation;
  animation.onfinish = () => {
    if (advancedAnimation !== animation) return;
    advancedAnimation = null;
    content.hidden = !expanded;
    animation.cancel();
    positionOpenPanel();
  };
}
function positionPopover(panel) {
  if (panel.hidden) return;
  const anchor = $("model-toggle");
  const anchorRect = anchor.getBoundingClientRect();
  const viewportWidth = document.documentElement.clientWidth;
  const viewportHeight = document.documentElement.clientHeight;
  const margin = viewportWidth <= 760 ? 10 : 12;
  const gap = 8;
  const preferredWidth = panel.id === "model-list-panel" ? 306 : 312;
  const width = Math.min(preferredWidth, viewportWidth - margin * 2);

  panel.style.width = `${width}px`;
  panel.style.left = `${Math.round(
    Math.min(
      Math.max(anchorRect.right - width, margin),
      viewportWidth - width - margin,
    ),
  )}px`;

  const panelHeight = panel.offsetHeight;
  const above = anchorRect.top - gap - panelHeight;
  const below = anchorRect.bottom + gap;
  const top =
    above >= margin
      ? above
      : Math.min(Math.max(below, margin), viewportHeight - panelHeight - margin);
  const boundedTop = Math.max(margin, top);
  panel.style.top = `${Math.round(boundedTop)}px`;
  panel.dataset.placement = boundedTop < anchorRect.top ? "above" : "below";
}
function positionOpenPanel() {
  const panel = $("model-list-panel").hidden
    ? $("model-panel")
    : $("model-list-panel");
  positionPopover(panel);
}
function openPanel(panel) {
  closePanel(true);
  cancelPanelClosure(panel);
  panel.classList.remove("popover-visible", "popover-closing");
  panel.hidden = false;
  $("model-toggle").setAttribute("aria-expanded", "true");
  if (panel.id === "model-list-panel")
    $("model-picker").setAttribute("aria-expanded", "true");
  positionPopover(panel);
  // 先提交初始状态，确保浏览器播放进入过渡而不是直接显示终态。
  void panel.offsetWidth;
  panel.classList.add("popover-visible");
}
function renderModelOptions() {
  const models = state.models;
  const hasSelectedModel = models.some((model) => model.name === state.model);
  $("model-options").replaceChildren();
  $("model-empty").hidden = Boolean(models.length);

  for (const [index, model] of models.entries()) {
    const selected = model.name === state.model;
    const option = document.createElement("button");
    option.type = "button";
    option.className = "model-option";
    option.dataset.model = model.name;
    option.setAttribute("role", "option");
    option.setAttribute("aria-selected", String(selected));
    option.tabIndex = selected || (!hasSelectedModel && index === 0) ? 0 : -1;

    const name = document.createElement("strong");
    name.className = "model-option-name";
    name.textContent = model.name;
    const check = document.createElement("span");
    check.className = "model-option-check";
    check.textContent = "✓";
    check.setAttribute("aria-hidden", "true");
    option.append(name, check);
    option.onclick = () => changeModel(model.name);
    $("model-options").append(option);
  }
  controls();
}
function changeModel(model) {
  if (!model || state.busy) return;
  if (model === state.model) {
    closePanel();
    return;
  }
  const old = state.model;
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
      closePanel();
    } catch (error) {
      state.model = old;
      renderModelOptions();
      throw error;
    }
  });
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
  updateModelControl();
  renderModelOptions();
}
function cacheSessionConfigs() {
  for (const session of state.sessions) {
    const config = session.model_config;
    if (!config || !session.model) continue;
    const normalized = normalizeModelConfig(config);
    if (normalized) state.configs.set(session.model, normalized);
  }
}
function cacheModelConfigs() {
  state.modelConfigs.clear();
  for (const model of state.models) {
    if (!model?.name) continue;
    const normalized = normalizeModelConfig(model.config);
    if (normalized) state.modelConfigs.set(model.name, normalized);
  }
}
async function refreshSessions() {
  state.sessions = list(await api("/api/sessions"));
  if (state.current && !current()) home();
  state.configs.clear();
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
  openDialog("name-dialog");
  $("name").focus();
  $("name").select();
}
async function createSession(name = "new session") {
  if (!state.model) throw new Error("请先选择可用模型");
  const config = effectiveConfig();
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
  state.configs.set(
    createdModel,
    normalizeModelConfig(data.model) || config,
  );
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
  openDialog("confirm-dialog");
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

  // A trailing pipe closes the row unless it is escaped as part of the cell.
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
      credentials: "include",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ session_id: id, message }),
    });
    if (!response.ok || !response.body) {
      const message = (await response.text()).slice(0, 160) || "流式请求失败";
      if (response.status === 401)
        showAuth("login", "登录状态已失效，请重新登录");
      throw new ApiError(message, response.status);
    }
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
  positionOpenPanel();
  controls();
}
function configPanel() {
  const customized = state.current
    ? state.configs.has(state.model)
    : Boolean(state.draftConfig);
  const config = effectiveConfig();
  $("temperature").value = config.temperature;
  $("tokens").value = config.max_tokens;
  $("think").checked = config.think;
  document.querySelectorAll("[name=effort]").forEach((input) => {
    input.checked = input.value === config.reasoning_effort;
  });
  syncThinkToggle();
  updateModelControl(config);
  renderModelOptions();
  $("config-note").textContent = customized
    ? "参数应用于同名模型。"
    : "当前显示模型建议值；点击应用后生效，影响同名模型。";
}
function normalizeUser(data) {
  const user = data?.user || data;
  return user?.user_id ? user : null;
}
function avatarUrl(path) {
  if (!path) return "./images/avatar.png";
  if (/^(?:https?:|data:)/i.test(path)) return path;
  // SDK 保存的是磁盘路径；浏览器访问时需要去掉静态资源根目录 ./www。
  const publicPath = path.startsWith("./www/") ? path.slice(5) : path;
  const url = state.base
    ? state.base +
      (publicPath.startsWith("/") ? publicPath : "/" + publicPath)
    : publicPath;
  if (!state.avatarRevision) return url;
  const separator = url.includes("?") ? "&" : "?";
  return `${url}${separator}avatar_v=${state.avatarRevision}`;
}
function updateUserUI() {
  if (!state.user) return;
  const name = state.user.user_name || "用户";
  const email = state.user.email || "未设置邮箱";
  const avatar = avatarUrl(state.user.avatar_path);
  $("user-name").textContent = name;
  $("user-email").textContent = email;
  $("user-avatar").src = avatar;
  $("account-name").textContent = name;
  $("account-email").textContent = email;
  $("account-avatar").src = avatar;
  $("account-id").textContent = state.user.user_id || "—";
  $("profile-name").value = state.user.user_name || "";
  $("settings-email").textContent = email;
}
function resetChatState() {
  state.models = [];
  state.sessions = [];
  state.current = "";
  state.model = "";
  state.menu = "";
  state.rename = null;
  state.avatarRevision = 0;
  state.deletion = null;
  state.configs.clear();
  state.modelConfigs.clear();
  state.drafts.clear();
  state.draftConfig = null;
  $("model-options").replaceChildren();
  $("messages").replaceChildren();
  $("message").value = "";
  $("welcome").hidden = false;
  $("history-error").hidden = true;
  closePanel(true);
  updateHeader();
  renderSessions();
  resize();
}
function setAuthMode(mode) {
  state.authMode = mode === "register" ? "register" : "login";
  const registering = state.authMode === "register";
  $("auth-tabs").dataset.mode = state.authMode;
  $("login-tab").setAttribute("aria-selected", String(!registering));
  $("register-tab").setAttribute("aria-selected", String(registering));
  $("login-form").hidden = registering;
  $("register-form").hidden = !registering;
  $("auth-title").textContent = registering ? "创建账号" : "欢迎回来";
  $("auth-subtitle").textContent = registering
    ? "几步即可开始新的对话"
    : "登录后继续你的对话";
  hideAllPasswords($(registering ? "register-form" : "login-form"));
  $(registering ? "register-name" : "login-email").focus();
}
function resetCodeButton(id) {
  const timer = codeTimers.get(id);
  if (timer) clearInterval(timer);
  codeTimers.delete(id);
  const button = $(id);
  delete button.dataset.cooldown;
  button.textContent = "发送验证码";
  button.disabled = state.busy;
}
function startCodeCountdown(id, seconds = 60) {
  resetCodeButton(id);
  const button = $(id);
  let remaining = Math.max(1, Math.ceil(Number(seconds) || 60));
  button.dataset.cooldown = "true";
  const render = () => {
    button.disabled = true;
    button.textContent = `${remaining} 秒后重发`;
  };
  render();
  const timer = setInterval(() => {
    remaining -= 1;
    if (remaining <= 0) {
      resetCodeButton(id);
      return;
    }
    render();
  }, 1000);
  codeTimers.set(id, timer);
}
async function sendEmailCode(email, purpose, buttonId) {
  if (!email) throw new Error("请先输入邮箱");
  try {
    const data = await api("/api/auth/email/code", "POST", { email, purpose });
    state.verifiedEmails.delete(purpose);
    startCodeCountdown(buttonId, data?.retry_after || 60);
  } catch (error) {
    if (error.status === 429 && error.data?.retry_after)
      startCodeCountdown(buttonId, error.data.retry_after);
    throw error;
  }
}
async function verifyEmailCode(email, purpose, code) {
  if (state.verifiedEmails.get(purpose) === email) return;
  if (!/^\d{6}$/.test(code)) throw new Error("请输入六位邮箱验证码");
  await api("/api/auth/email/verify", "POST", { email, purpose, code });
  state.verifiedEmails.set(purpose, email);
}
function openVerification(email, purpose, onVerified) {
  state.pendingVerification = { email, purpose, onVerified };
  state.verifiedEmails.delete(purpose);
  $("verification-code").value = "";
  $("verification-status").textContent = "";
  $("verification-status").classList.remove("error");
  $("verification-description").textContent = `验证码将发送到 ${email}`;
  resetCodeButton("verification-send-code");
  openDialog("verification-dialog");
}
function closeAccountChangeDialog(id) {
  closeDialog(id, () => {
    if (state.user && document.body.classList.contains("authenticated")) {
      updateUserUI();
      openDialog("account-dialog");
    }
  });
}
async function verificationAction(label, work) {
  if (state.busy) return;
  state.busy = true;
  $("verification-status").textContent = label;
  $("verification-status").classList.remove("error");
  controls();
  try {
    await work();
  } catch (error) {
    $("verification-status").textContent = error.message || "验证失败，请重试";
    $("verification-status").classList.add("error");
  } finally {
    state.busy = false;
    controls();
  }
}
function showAuth(mode = "login", message = "") {
  state.user = null;
  state.pendingVerification = null;
  document.body.classList.remove("authenticated", "auth-pending");
  document.body.classList.add("auth-required");
  document.body.classList.remove("mobile-open");
  $("backdrop").hidden = true;
  document
    .querySelectorAll("dialog[open]")
    .forEach((dialog) => closeDialogImmediately(dialog));
  $("auth-status").textContent = message;
  $("auth-status").classList.remove("error");
  $("login-password").value = "";
  $("register-password").value = "";
  $("register-confirm").value = "";
  $("register-code").value = "";
  state.verifiedEmails.clear();
  resetChatState();
  setAuthMode(mode);
}
async function showApp(user) {
  state.user = user;
  document.body.classList.remove("auth-required", "auth-pending");
  document.body.classList.add("authenticated");
  $("auth-status").textContent = "";
  $("auth-status").classList.remove("error");
  updateUserUI();
  try {
    await connect();
  } catch (error) {
    toast(error.message || "会话加载失败，请稍后重试");
  }
}
async function authAction(label, work) {
  if (state.busy) return;
  state.busy = true;
  $("auth-status").textContent = label;
  $("auth-status").classList.remove("error");
  controls();
  try {
    await work();
  } catch (error) {
    $("auth-status").textContent = error.message || "请求失败，请重试";
    $("auth-status").classList.add("error");
  } finally {
    state.busy = false;
    controls();
  }
}
async function forgotPasswordAction(label, work) {
  if (state.busy) return;
  state.busy = true;
  $("forgot-password-status").textContent = label;
  $("forgot-password-status").classList.remove("error");
  controls();
  try {
    await work();
  } catch (error) {
    $("forgot-password-status").textContent =
      error.message || "请求失败，请重试";
    $("forgot-password-status").classList.add("error");
  } finally {
    state.busy = false;
    controls();
  }
}
async function connect() {
  connection(false, "正在连接");
  try {
    const [models, sessions] = await Promise.all([
      api("/api/models"),
      api("/api/sessions"),
    ]);
    state.models = list(models);
    cacheModelConfigs();
    state.sessions = list(sessions);
    state.configs.clear();
    cacheSessionConfigs();
    if (!state.models.some((m) => m.name === state.model))
      state.model = state.models[0]?.name || "";
    renderModelOptions();
    connection(true, "已连接");
    updateHeader();
    renderSessions();
  } catch (error) {
    connection(false, "连接失败 · 点击重试");
    throw error;
  }
}
async function bootstrap() {
  connection(false, "正在连接");
  try {
    const user = normalizeUser(
      await api("/api/auth/info", "GET", undefined, {
        allowUnauthorized: true,
      }),
    );
    if (!user) throw new ApiError("用户信息无效", 401);
    await showApp(user);
  } catch (error) {
    const message =
      error.status === 401 ? "" : error.message || "暂时无法连接服务器";
    showAuth("login", message);
  }
}

$("theme").onclick = () =>
  theme(document.documentElement.dataset.theme === "dark" ? "light" : "dark");
$("auth-theme").onclick = $("theme").onclick;
$("login-tab").onclick = () => {
  $("auth-status").textContent = "";
  $("auth-status").classList.remove("error");
  setAuthMode("login");
};
$("register-tab").onclick = () => {
  $("auth-status").textContent = "";
  $("auth-status").classList.remove("error");
  setAuthMode("register");
};
$("register-email").oninput = () => {
  state.verifiedEmails.delete("register");
  $("register-code").value = "";
  resetCodeButton("register-send-code");
};
$("register-code").oninput = () => {
  state.verifiedEmails.delete("register");
};
$("register-send-code").onclick = () => {
  if (!$("register-email").reportValidity()) return;
  authAction("正在发送验证码…", async () => {
    await sendEmailCode(
      $("register-email").value.trim(),
      "register",
      "register-send-code",
    );
    $("auth-status").textContent = "验证码已发送，请检查邮箱";
  });
};
$("login-form").onsubmit = (event) => {
  event.preventDefault();
  authAction("正在登录…", async () => {
    const user = normalizeUser(
      await api("/api/auth/login", "POST", {
        email: $("login-email").value.trim(),
        password: $("login-password").value,
      }),
    );
    if (!user) throw new Error("登录响应缺少用户信息");
    await showApp(user);
  });
};
$("open-forgot-password").onclick = () => {
  $("forgot-password-form").reset();
  $("forgot-password-email").value = $("login-email").value.trim();
  $("forgot-password-status").textContent = "";
  $("forgot-password-status").classList.remove("error");
  resetCodeButton("forgot-password-send-code");
  openDialog("forgot-password-dialog");
  $("forgot-password-email").focus();
};
$("forgot-password-email").oninput = () => {
  $("forgot-password-code").value = "";
  resetCodeButton("forgot-password-send-code");
};
$("forgot-password-send-code").onclick = () => {
  if (!$("forgot-password-email").reportValidity()) return;
  forgotPasswordAction("正在发送验证码…", async () => {
    await sendEmailCode(
      $("forgot-password-email").value.trim(),
      "forget_password",
      "forgot-password-send-code",
    );
    $("forgot-password-status").textContent = "验证码已发送，请检查邮箱";
  });
};
$("forgot-password-form").onsubmit = (event) => {
  event.preventDefault();
  forgotPasswordAction("正在重置密码…", async () => {
    const email = $("forgot-password-email").value.trim();
    const code = $("forgot-password-code").value.trim();
    const password = $("forgot-password-value").value;
    if (!/^\d{6}$/.test(code)) throw new Error("请输入六位邮箱验证码");
    if (password !== $("forgot-password-confirm").value)
      throw new Error("两次输入的新密码不一致");

    // 验证码由重置接口验证并立即消费，不能在这里提前调用通用验证接口。
    await api("/api/auth/forget_password", "POST", {
      email,
      code,
      password,
    });
    $("login-email").value = email;
    $("login-password").value = "";
    closeDialog("forgot-password-dialog", () => {
      $("auth-status").textContent = "密码已重置，请使用新密码登录";
      $("auth-status").classList.remove("error");
      $("login-password").focus();
    });
  });
};
$("register-form").onsubmit = (event) => {
  event.preventDefault();
  authAction("正在创建账号…", async () => {
    const email = $("register-email").value.trim();
    const password = $("register-password").value;
    const code = $("register-code").value.trim();
    if (password !== $("register-confirm").value)
      throw new Error("两次输入的密码不一致");
    if (!/^\d{6}$/.test(code)) throw new Error("请输入六位邮箱验证码");
    // 注册接口会原子地验证并消费验证码，不能提前调用通用验证接口。
    const user = normalizeUser(
      await api("/api/auth/register", "POST", {
        user_name: $("register-name").value.trim(),
        email,
        password,
        code,
        purpose: "register",
      }),
    );
    if (!user) throw new Error("注册响应缺少用户信息");
    await showApp(user);
  });
};
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
$("user-menu").onclick = () => {
  if (!state.user || state.busy) return;
  updateUserUI();
  openDialog("account-dialog");
};
$("avatar-upload").onclick = () => {
  if (state.busy) return;
  $("avatar-file").value = "";
  $("avatar-file").click();
};
$("avatar-file").onchange = () => {
  const file = $("avatar-file").files?.[0];
  $("avatar-file").value = "";
  if (!file) return;
  if (!supportedAvatarTypes.has(file.type)) {
    toast("头像仅支持 PNG 或 JPEG 格式");
    return;
  }
  if (!file.size) {
    toast("不能上传空图片");
    return;
  }
  if (file.size > maximumAvatarBytes) {
    toast("头像大小不能超过 5MB");
    return;
  }
  openAvatarConfirmation(file);
};
$("avatar-confirm-form").onsubmit = (event) => {
  event.preventDefault();
  const file = state.pendingAvatarFile;
  if (!file) {
    closeDialog("avatar-confirm-dialog");
    return;
  }
  action("上传头像…", async () => {
    await uploadAvatar(file);
    const user = normalizeUser(await api("/api/auth/info"));
    if (!user) throw new Error("头像已上传，但用户信息刷新失败，请刷新页面");
    state.user = user;
    // 后端复用同一头像 URL，增加版本参数以绕过浏览器旧缓存。
    state.avatarRevision = Date.now();
    updateUserUI();
    closeDialog("avatar-confirm-dialog");
    toast("头像已更新");
  });
};
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
    closeDialog("name-dialog");
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
    closeDialog("confirm-dialog");
    toast(id === "*" ? "全部会话已清空" : "会话已删除");
  });
};
$("model-toggle").onclick = () => {
  if ($("model-panel").classList.contains("popover-visible")) {
    closePanel();
    return;
  }
  configPanel();
  openPanel($("model-panel"));
};
$("model-picker").onclick = () => {
  renderModelOptions();
  openPanel($("model-list-panel"));
  $("model-options")
    .querySelector('[aria-selected="true"], .model-option')
    ?.focus();
};
$("reset-parameters").onclick = () => {
  configPanel();
  positionOpenPanel();
};
$("think-toggle").onclick = () => {
  $("think").checked = !$("think").checked;
  syncThinkToggle();
};
$("model-options").onkeydown = (event) => {
  const option = event.target.closest(".model-option");
  if (!option) return;
  const options = [...$("model-options").querySelectorAll(".model-option")];
  const index = options.indexOf(option);
  let next = null;
  if (event.key === "ArrowDown") next = options[(index + 1) % options.length];
  if (event.key === "ArrowUp")
    next = options[(index - 1 + options.length) % options.length];
  if (event.key === "Home") next = options[0];
  if (event.key === "End") next = options[options.length - 1];
  if (!next) return;
  event.preventDefault();
  next.focus();
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
    updateModelControl(config);
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
  openDialog("connection-dialog");
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
    $("model-options").replaceChildren();
    state.sessions = [];
    state.configs.clear();
    state.modelConfigs.clear();
    state.drafts.clear();
    state.draftConfig = null;
    $("message").value = "";
    home();
    await connect();
    closeDialog("connection-dialog");
  });
};
$("profile-form").onsubmit = (event) => {
  event.preventDefault();
  action("保存用户名…", async () => {
    const userName = $("profile-name").value.trim();
    if (!userName) throw new Error("用户名不能为空");
    if (userName === state.user.user_name) throw new Error("用户名没有变化");
    await api("/api/user/name", "POST", { user_name: userName });
    state.user.user_name = userName;
    updateUserUI();
    toast("用户名已更新");
  });
};
$("open-email-change").onclick = () => {
  const currentEmail = state.user.email;
  closeDialog("account-dialog", () => {
    openVerification(currentEmail, "change_email", () => {
      $("change-email-value").value = "";
      openDialog("change-email-dialog");
    });
  });
};
$("change-email-form").onsubmit = (event) => {
  event.preventDefault();
  const email = $("change-email-value").value.trim();
  if (email === state.user.email) {
    toast("新邮箱不能与当前邮箱相同");
    return;
  }
  action("更新邮箱…", async () => {
    await api("/api/user/email", "POST", { email });
    state.user.email = email;
    updateUserUI();
    closeAccountChangeDialog("change-email-dialog");
    toast("邮箱已更新");
  });
};
$("open-password-change").onclick = () => {
  const currentEmail = state.user.email;
  closeDialog("account-dialog", () => {
    openVerification(currentEmail, "change_password", () => {
      $("change-password-form").reset();
      openDialog("change-password-dialog");
    });
  });
};
$("change-password-form").onsubmit = (event) => {
  event.preventDefault();
  const password = $("new-password").value;
  if (password !== $("confirm-password").value) {
    toast("两次输入的密码不一致");
    return;
  }
  action("更新密码…", async () => {
    await api("/api/user/password", "POST", { password });
    $("change-password-form").reset();
    closeAccountChangeDialog("change-password-dialog");
    toast("密码已更新");
  });
};
$("verification-send-code").onclick = () => {
  const pending = state.pendingVerification;
  if (!pending) return;
  verificationAction("正在发送验证码…", async () => {
    await sendEmailCode(
      pending.email,
      pending.purpose,
      "verification-send-code",
    );
    $("verification-status").textContent = "验证码已发送，请检查邮箱";
  });
};
$("verification-code").oninput = () => {
  const pending = state.pendingVerification;
  if (pending) state.verifiedEmails.delete(pending.purpose);
};
$("verification-form").onsubmit = (event) => {
  event.preventDefault();
  verificationAction("正在验证…", async () => {
    const pending = state.pendingVerification;
    if (!pending) throw new Error("当前没有待验证的操作");
    await verifyEmailCode(
      pending.email,
      pending.purpose,
      $("verification-code").value.trim(),
    );
    const onVerified = pending.onVerified;
    state.verifiedEmails.delete(pending.purpose);
    state.pendingVerification = null;
    closeDialog("verification-dialog", onVerified);
  });
};
for (const id of ["cancel-email-change", "cancel-email-change-x"])
  $(id).onclick = () => closeAccountChangeDialog("change-email-dialog");
for (const id of ["cancel-password-change", "cancel-password-change-x"])
  $(id).onclick = () => closeAccountChangeDialog("change-password-dialog");
function cancelVerification() {
  state.pendingVerification = null;
  closeDialog("verification-dialog", () => {
    if (state.user && document.body.classList.contains("authenticated")) {
      updateUserUI();
      openDialog("account-dialog");
    }
  });
}
for (const id of ["cancel-verification", "cancel-verification-x"])
  $(id).onclick = cancelVerification;
for (const id of ["cancel-forgot-password", "cancel-forgot-password-x"])
  $(id).onclick = () => closeDialog("forgot-password-dialog");
$("forgot-password-dialog").addEventListener("close", () => {
  $("forgot-password-form").reset();
  $("forgot-password-status").textContent = "";
  $("forgot-password-status").classList.remove("error");
  resetCodeButton("forgot-password-send-code");
});
$("verification-dialog").addEventListener("close", () => {
  state.pendingVerification = null;
  $("verification-code").value = "";
  $("verification-status").textContent = "";
  resetCodeButton("verification-send-code");
});
$("avatar-confirm-dialog").addEventListener("close", clearPendingAvatar);
for (const [id, close] of [
  ["change-email-dialog", () => closeAccountChangeDialog("change-email-dialog")],
  [
    "change-password-dialog",
    () => closeAccountChangeDialog("change-password-dialog"),
  ],
]) {
  $(id).addEventListener("cancel", (event) => {
    if (state.busy) return;
    event.preventDefault();
    close();
  });
}
$("verification-dialog").addEventListener("cancel", (event) => {
  if (state.busy) return;
  event.preventDefault();
  cancelVerification();
});
$("logout").onclick = () => {
  action("正在退出…", async () => {
    await api("/api/auth/logout", "POST");
    showAuth("login", "你已安全退出");
  });
};
document.querySelectorAll("[data-close]").forEach((button) => {
  button.onclick = () => closeDialog(button.closest("dialog"));
});
document.querySelectorAll("dialog").forEach((dialog) => {
  dialog.addEventListener("cancel", (event) => {
    if (event.defaultPrevented) return;
    event.preventDefault();
    if (!state.busy) closeDialog(dialog);
  });
});
document.addEventListener("click", (event) => {
  if (
    !state.busy &&
    !$("model-panel").contains(event.target) &&
    !$("model-list-panel").contains(event.target) &&
    !$("model-toggle").contains(event.target)
  )
    closePanel();
});
window.addEventListener("resize", positionOpenPanel);
window.addEventListener("scroll", positionOpenPanel, true);
const popoverResizeObserver =
  typeof ResizeObserver === "function"
    ? new ResizeObserver(() => positionOpenPanel())
    : null;
popoverResizeObserver?.observe($("model-panel"));
popoverResizeObserver?.observe($("model-list-panel"));
$("advanced-toggle").onclick = () => {
  setAdvancedExpanded(
    $("advanced-toggle").getAttribute("aria-expanded") !== "true",
  );
};
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
initializePasswordToggles();
initializeSidebarResizer();
resize();
bootstrap();
