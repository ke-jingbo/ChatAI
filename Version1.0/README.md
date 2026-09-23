# ChatAI SDK / AIChatServer

ChatAI 是一个基于 C++17 实现的大语言模型聊天项目。项目由可复用的聊天 SDK、HTTP 服务端和原生 Web 前端组成，支持接入多个云端或本地大模型，并通过 SQLite 持久化会话与消息。

当前目录为 **Version 1.0.0**。这一版本主要完成单用户场景下的模型接入、会话管理、消息收发和网页交互；账号注册、登录与多用户数据隔离计划在 Version 2.0 中实现。

## 主要功能

- 支持 DeepSeek、MiMo、Kimi 和本地 Ollama Provider
- 支持通过配置文件注册模型及设置默认模型参数
- 支持创建、重命名、切换、删除和清空会话
- 支持切换会话模型以及调整温度、Max tokens、思考模式等参数
- 支持普通响应和基于 SSE 的流式响应
- 流式输出过程中实时解析 Markdown
- 支持标题、列表、引用、行内格式和围栏代码块等 Markdown 内容
- 代码块显示语言标识、生成状态，并提供一键复制
- 使用 SQLite 持久化会话和聊天记录
- Web 前端支持会话搜索、明暗主题、侧边栏收起/展开和移动端布局
- 支持服务地址、端口、日志、数据库和模型配置文件等启动参数

## 项目结构

```text
Version1.0/
├── SDK/                         # 核心 C++ SDK
│   ├── include/                 # 对外头文件
│   ├── src/                     # SDK 与各 Provider 实现
│   └── CMakeLists.txt
├── ChatServer/                  # HTTP 服务端
│   ├── ChatServer.cpp
│   ├── ChatServer.h
│   ├── server.cpp               # 程序入口、配置解析和启动参数
│   ├── bin/
│   │   ├── config.json          # Provider 与模型配置示例
│   │   └── www/                 # 原生 HTML/CSS/JavaScript 前端
│   └── CMakeLists.txt
└── Debug/                       # SDK 调试与测试代码
```

核心调用关系：

```text
Web UI
  │ HTTP / SSE
  ▼
AIChatServer
  ▼
ChatSDK
  ├── LLMManager ──► DeepSeek / MiMo / Kimi / Ollama
  └── SessionManager ──► DataManager ──► SQLite
```

## 环境要求

- Linux（当前 CMake 安装与查找路径以 `/usr/local` 为默认位置）
- 支持 C++17 的编译器
- CMake 3.10 或更高版本
- OpenSSL
- SQLite3
- JsonCpp
- cpp-httplib
- gflags
- spdlog
- fmt
- pthread/Threads

在 Ubuntu/Debian 上可以参考以下命令安装依赖，具体包名可能随发行版版本不同而变化：

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config \
  libssl-dev libsqlite3-dev libjsoncpp-dev \
  libgflags-dev libspdlog-dev libfmt-dev libcpp-httplib-dev
```

## 构建

服务端会从 `/usr/local` 查找已安装的 `ai_chat_sdk`，因此需要先构建并安装 SDK，再构建 ChatServer。

### 1. 构建并安装 SDK

```bash
cd Version1.0/SDK
cmake -S . -B build
cmake --build build -j
sudo cmake --install build
```

安装完成后，默认会生成：

```text
/usr/local/lib/libai_chat_sdk.a
/usr/local/include/ai_chat_sdk/
```

### 2. 构建 ChatServer

```bash
cd ../ChatServer
cmake -S . -B build
cmake --build build -j
```

可执行文件将输出到：

```text
Version1.0/ChatServer/bin/AIChatServer
```

## 配置模型

默认配置文件位于 [`ChatServer/bin/config.json`](ChatServer/bin/config.json)。`providers` 用于配置模型服务提供者，`models` 用于注册可在 SDK 和网页端选择的模型。

云端 Provider 推荐使用 `api_key_env` 指定环境变量名称，不要将 API Key 直接提交到配置文件中。例如：

```json
{
  "providers": [
    {
      "provider": "DeepseekProvider",
      "api_key_env": "deepseek_apikey"
    },
    {
      "provider": "OllamaLLMProvider",
      "name": "qwen3:0.6b",
      "desc": "Local Qwen3 0.6B model",
      "path": "http://127.0.0.1:11434"
    }
  ],
  "models": [
    {
      "name": "deepseek-flash",
      "provider": "DeepseekProvider",
      "desc": "DeepSeek Flash",
      "config": {
        "temperature": 0.8,
        "max_tokens": 393216,
        "think": true,
        "reasoning_effort": "high"
      }
    }
  ]
}
```

示例配置使用以下环境变量：

```bash
export deepseek_apikey="your-deepseek-api-key"
export mimo_apikey="your-mimo-api-key"
export kimi_apikey="your-kimi-api-key"
```

只使用本地 Ollama 时，可以从配置文件中移除不需要的云端 Provider 和对应模型。

## 启动服务

服务端通过相对路径加载 `config.json` 和 `www`，因此建议从 `ChatServer/bin` 目录启动：

```bash
cd Version1.0/ChatServer/bin
./AIChatServer --host=127.0.0.1 --port=8080 --config=./config.json
```

启动后访问：

```text
http://127.0.0.1:8080
```

查看帮助或版本：

```bash
./AIChatServer --help
./AIChatServer --version
```

常用启动参数：

| 参数 | 默认值 | 说明 |
| --- | --- | --- |
| `--host` | `0.0.0.0` | HTTP 服务监听地址 |
| `--port` | `8080` | HTTP 服务监听端口 |
| `--log_path` | `./logs` | 日志文件、日志目录或 `stdout` |
| `--log_level` | `info` | 日志等级 |
| `--db_name` | `chat_server.db` | SQLite 数据库文件路径 |
| `--config` | `./config.json` | Provider 与模型配置文件路径 |

## HTTP API

| 方法 | 路径 | 功能 |
| --- | --- | --- |
| `GET` | `/api/models` | 获取可用模型 |
| `GET` | `/api/sessions` | 获取会话列表 |
| `POST` | `/api/session` | 创建会话 |
| `DELETE` | `/api/session/{session_id}` | 删除指定会话 |
| `DELETE` | `/api/sessions` | 清空全部会话 |
| `GET` | `/api/session/{session_id}/history` | 获取会话历史消息 |
| `POST` | `/api/session/name` | 修改会话名称 |
| `POST` | `/api/session/model` | 切换会话模型 |
| `POST` | `/api/session/model_config` | 修改模型参数 |
| `POST` | `/api/message` | 发送消息并等待完整响应 |
| `POST` | `/api/message/async` | 发送消息并通过 SSE 流式返回 |

创建会话请求示例：

```json
{
  "session_name": "new session",
  "model": {
    "model_name": "deepseek-flash",
    "temperature": 0.8,
    "max_tokens": 393216,
    "think": true,
    "reasoning_effort": "high"
  }
}
```

## SDK 能力

`ChatSDK` 对外提供以下核心能力：

- 初始化 Provider 和模型
- 查询可用模型及其配置
- 创建、查询、更新和删除会话
- 清空全部会话
- 同步发送消息
- 通过回调接收流式消息

对外头文件位于 [`SDK/include/ChatSDK.h`](SDK/include/ChatSDK.h)。

## Version 1.0 使用范围

Version 1.0 尚未实现用户注册、登录和身份鉴权。当前所有会话与消息共享同一个服务端数据空间，更适合本地使用、开发调试或可信环境中的单用户部署。

如无额外的反向代理鉴权，请不要直接将本版本暴露到公网。仅在本机使用时，建议通过 `--host=127.0.0.1` 启动。

## Version 2.0 计划

Version 2.0 将在现有 `SessionManager` 之上加入用户管理能力，计划包括：

- 用户注册、登录与退出
- 密码安全哈希与数据库存储
- 生成随机且唯一的登录 Session ID
- 通过安全 Cookie 维持登录状态
- 会话和消息记录关联 `user_id`
- 每个用户拥有独立的会话管理空间
- 服务端鉴权与接口访问控制

Version 1.0 会继续作为不包含用户系统的基础版本保留。
