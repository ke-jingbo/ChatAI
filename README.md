# ChatAI-SDK

ChatAI-SDK 是一个使用 C++17 实现的多模型对话项目，包含可复用的聊天 SDK、HTTP/SSE 服务端和原生 Web 前端。项目支持 DeepSeek、MiMo、Kimi 与本地 Ollama，使用 SQLite 持久化用户、会话和消息。

仓库保留了两个完整版本：`Version1.0` 用于单用户聊天场景，`Version2.0` 在其基础上加入账号认证、多用户数据隔离、邮箱验证码和用户资料管理。新部署建议从 **Version 2.0.0** 开始。

## 版本概览

| 版本 | 定位 | 主要能力 | 文档 |
| --- | --- | --- | --- |
| Version 1.0.0 | 单用户基础版 | 多模型接入、会话管理、SQLite 持久化、同步与 SSE 流式对话、Web UI | [Version1.0/README.md](Version1.0/README.md) |
| Version 2.0.0 | 多用户完整版本 | 继承 V1 能力，并增加注册登录、邮箱验证、Cookie 会话、用户资料和数据隔离 | [Version2.0/README.md](Version2.0/README.md) |

> Version 1.0 没有身份鉴权，适合本地开发或可信的单用户环境，不应直接暴露到公网。

## 核心功能

- 统一接入 DeepSeek、MiMo、Kimi 和 Ollama Provider
- 通过 JSON 注册模型，并配置温度、最大 token 数、思考模式和推理强度
- 创建、重命名、切换、删除和搜索会话
- 支持普通响应和基于 SSE 的流式响应
- 使用 SQLite 持久化用户、会话与消息记录
- 原生 HTML/CSS/JavaScript 前端，无前端构建步骤
- Markdown、代码块复制、表格、明暗主题和响应式布局
- Version 2.0 支持注册、登录、退出、找回密码和邮箱验证码
- Version 2.0 支持用户名、邮箱、密码和头像管理
- Version 2.0 按用户隔离会话及聊天记录

## 架构

```text
Browser Web UI
      │
      │ HTTP / JSON / SSE
      ▼
AIChatServer (cpp-httplib)
      │
      ▼
ChatSDK
  ├── UserManager ─────────────────────┐   Version 2.0
  ├── SessionManager ──► DataManager ──┴──► SQLite
  └── LLMManager
        ├── DeepseekProvider ──────────────► DeepSeek API
        ├── MimoProvider ─────────────────► MiMo API
        ├── KimiProvider ─────────────────► Kimi API
        └── OllamaLLMProvider ────────────► Local Ollama
```

一次对话的主要调用链是：

```text
Web 请求 → ChatServer 鉴权与参数校验 → ChatSDK
        → SessionManager 读取上下文 → LLMManager 选择 Provider
        → 模型生成回复 → DataManager 保存消息 → HTTP/SSE 返回前端
```

## 项目结构

```text
ChatAI/
├── Version1.0/
│   ├── SDK/                  # 单用户核心 SDK
│   ├── ChatServer/           # V1 HTTP 服务与 Web 前端
│   └── Debug/                # SDK、Provider 和 SQLite 调试程序
├── Version2.0/
│   ├── SDK/                  # 增加 UserManager 的多用户 SDK
│   ├── ChatServer/
│   │   ├── include/          # 服务端与邮箱验证接口
│   │   ├── src/              # 服务入口、HTTP 路由与邮箱验证实现
│   │   └── bin/
│   │       ├── config.json   # Provider 与模型配置
│   │       └── www/          # 原生 Web 前端
│   └── Test/                 # 验证码和 QQ SMTP 测试
└── README.md                 # 仓库总览
```

## 环境要求

- Linux
- 支持 C++17 的编译器
- CMake 3.10 或更高版本
- OpenSSL
- SQLite3
- JsonCpp
- cpp-httplib
- gflags
- spdlog 与 fmt
- libsodium（Version 2.0 密码摘要）
- libcurl（Version 2.0 QQ SMTP 邮件发送）
- pthread / Threads
- Ollama（仅使用本地模型时需要）

Ubuntu 可参考：

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
  libssl-dev libsqlite3-dev libjsoncpp-dev libcpp-httplib-dev \
  libgflags-dev libspdlog-dev libfmt-dev libsodium-dev libcurl4-openssl-dev
```

包名可能随发行版变化。当前 CMake 配置默认从 `/usr/local` 查找已安装的 SDK。

## 快速开始：Version 2.0

### 1. 准备模型配置

默认配置位于 [`Version2.0/ChatServer/bin/config.json`](Version2.0/ChatServer/bin/config.json)。云端 Provider 的 `api_key_env` 保存的是环境变量名称，而不是密钥本身：

```bash
export deepseek_apikey='your-deepseek-api-key'
export mimo_apikey='your-mimo-api-key'
export kimi_apikey='your-kimi-api-key'
```

如果只使用本地 Ollama，请从配置文件移除不使用的云端 Provider 及其模型，并确认 Ollama 服务可访问：

```bash
ollama serve
```

不要把真实 API Key 写入配置文件或提交到 Git。

### 2. 构建并安装 SDK

```bash
cmake -S Version2.0/SDK -B Version2.0/SDK/build -DCMAKE_BUILD_TYPE=Release
cmake --build Version2.0/SDK/build -j2
sudo cmake --install Version2.0/SDK/build
```

默认安装位置：

```text
/usr/local/lib/libai_chat_sdk.a
/usr/local/include/ai_chat_sdk/
```

### 3. 构建 ChatServer

```bash
cmake -S Version2.0/ChatServer -B Version2.0/ChatServer/build -DCMAKE_BUILD_TYPE=Release
cmake --build Version2.0/ChatServer/build -j2
```

生成的可执行文件位于：

```text
Version2.0/ChatServer/bin/AIChatServer
```

### 4. 配置邮箱验证码（可选）

注册和找回密码的验证码通过 QQ SMTP 发送：

```bash
export QQ_SMTP_USER='your-account@qq.com'
export QQ_SMTP_AUTH_CODE='your-smtp-authorization-code'
```

这里应使用 QQ 邮箱生成的 SMTP 授权码。未配置时，聊天服务仍可启动，但发送验证码不可用。

### 5. 启动服务

服务端使用相对路径加载 `config.json`、数据库、日志和 `www`，因此建议从 `bin` 目录启动：

```bash
cd Version2.0/ChatServer/bin
./AIChatServer \
  --host=127.0.0.1 \
  --port=8080 \
  --config=./config.json \
  --db_name=./chat_server.db \
  --log_path=./logs
```

浏览器访问：<http://127.0.0.1:8080/>

查看命令行帮助和版本：

```bash
./AIChatServer --help
./AIChatServer --version
```

## 模型配置

一个模型由 Provider、展示信息和推理参数组成：

```json
{
  "name": "mimo-v2.5-pro",
  "provider": "MimoProvider",
  "desc": "MiMo v2.5 Pro",
  "config": {
    "temperature": 0.8,
    "max_tokens": 131072,
    "think": true,
    "reasoning_effort": "high"
  }
}
```

当前服务入口会校验配置：MiMo 的 `temperature` 范围为 `0–1.5`、`max_tokens` 范围为 `1–131072`；其他 Provider 的对应范围为 `0–2` 和 `1–393216`。前端应使用接口返回的模型配置，不要为所有模型写死相同上限。

## Version 2.0 API 总览

| 分类 | 方法与路径 | 功能 |
| --- | --- | --- |
| 模型 | `GET /api/models` | 获取可用模型 |
| 认证 | `POST /api/auth/email/code` | 发送邮箱验证码 |
| 认证 | `POST /api/auth/email/verify` | 校验邮箱验证码 |
| 认证 | `POST /api/auth/register` | 注册并登录 |
| 认证 | `POST /api/auth/login` | 登录 |
| 认证 | `POST /api/auth/logout` | 退出登录 |
| 认证 | `GET /api/auth/info` | 获取当前用户 |
| 认证 | `POST /api/auth/forget_password` | 通过验证码重置密码 |
| 用户 | `POST /api/user/name` | 修改用户名 |
| 用户 | `POST /api/user/email` | 修改邮箱 |
| 用户 | `POST /api/user/password` | 修改密码 |
| 用户 | `POST /api/user/avatar` | 上传头像原始二进制 |
| 会话 | `GET /api/sessions` | 获取当前用户的会话列表 |
| 会话 | `POST /api/session` | 创建会话 |
| 会话 | `DELETE /api/session/{session_id}` | 删除会话 |
| 会话 | `DELETE /api/sessions` | 清空当前用户的会话 |
| 会话 | `GET /api/session/{session_id}/history` | 获取历史消息 |
| 会话 | `POST /api/session/name` | 修改会话名称 |
| 会话 | `POST /api/session/model` | 切换模型 |
| 会话 | `POST /api/session/model_config` | 修改模型参数 |
| 消息 | `POST /api/message` | 获取完整回复 |
| 消息 | `POST /api/message/async` | 通过 SSE 流式获取回复 |

完整请求体、响应格式和 `curl` 示例见 [Version2.0/README.md](Version2.0/README.md)。V1 的历史接口文档见 [Version1.0/ChatServer/ChatServerApi.md](Version1.0/ChatServer/ChatServerApi.md)。

## 数据与安全设计

Version 2.0 当前已经实现：

- 使用 libsodium `crypto_pwhash_str` 保存密码摘要，不保存明文密码
- 使用 OpenSSL 安全随机源生成登录 Cookie ID 和六位验证码
- 验证码在内存中保存 HMAC-SHA256 摘要，并使用恒定时间比较
- 验证码绑定邮箱与用途，带有效期、尝试次数和一分钟重发限制
- Cookie 设置 `HttpOnly`、`SameSite=Lax` 和有效期
- 会话操作通过 `user_id` 做用户数据隔离
- SQLite 启用外键约束
- 头像限制为 PNG/JPEG，最大 5 MiB

## 测试

Version 2.0 提供验证码逻辑测试和 QQ SMTP 联调程序：

```bash
cmake -S Version2.0/Test -B Version2.0/Test/build
cmake --build Version2.0/Test/build -j2
./Version2.0/Test/build/verification_code_test
```

`qq_smtp_test` 会连接真实 SMTP 服务并发送邮件，只应在明确设置测试收件人和凭据后手动运行：

```bash
./Version2.0/Test/build/qq_smtp_test recipient@example.com
```

Version 1.0 的 SDK/Provider 调试入口位于 [`Version1.0/Debug`](Version1.0/Debug)。云端 Provider 测试会消耗真实 API 配额，运行前请检查配置和密钥。

## 当前复盘

项目已经完成从“模型调用封装”到“可实际使用的多用户聊天应用”的两阶段演进：

1. **Version 1.0：打通核心链路。** 完成统一 Provider 接口、模型与会话管理、SQLite 持久化、HTTP/SSE 服务和 Web 对话界面。
2. **Version 2.0：补齐用户闭环。** 引入用户表、密码摘要、登录 Cookie、邮箱验证、用户资料及按用户隔离的会话访问。

当前仓库适合作为 C++ 大模型接入、流式聊天服务以及从单用户架构演进到多用户架构的完整实践项目。
