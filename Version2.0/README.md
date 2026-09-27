# ChatAI-SDK Version 2.0.0

ChatAI-SDK Version 2.0.0 是一个带 Web 前端的多模型对话服务。项目由两部分组成：

- `SDK`：C++17 聊天 SDK，负责用户、会话、模型和消息等核心能力。
- `ChatServer`：基于 HTTP 的服务端，提供认证、用户设置、会话管理、模型查询和流式对话 API，并在同一端口托管 `ChatServer/bin/www` 前端。

## 目录

```text
Version2.0/
├── SDK/                 C++17 静态库
├── ChatServer/          HTTP 服务
│   ├── bin/config.json  Provider 和模型配置
│   └── bin/www/         Web 前端静态文件
└── Test/                测试代码
```

## 运行要求

- CMake 3.10 或更高版本
- 支持 C++17 的编译器
- OpenSSL、Threads、gflags、spdlog、jsoncpp、libsodium、libcurl、SQLite3
- 可安装到 `/usr/local/include/ai_chat_sdk` 和 `/usr/local/lib` 的 SDK 权限
- 使用云端模型时，需要准备对应 Provider 的 API Key；本地 Ollama 模型还需要运行 Ollama 服务

依赖名称会随发行版不同而变化，请按当前系统的包管理器安装。项目不会把 API Key 写入配置文件，而是从环境变量读取。

## 构建

先构建并安装 SDK，再构建 ChatServer：

```bash
cmake -S SDK -B SDK/build -DCMAKE_BUILD_TYPE=Release
cmake --build SDK/build -j2
sudo cmake --install SDK/build

cmake -S ChatServer -B ChatServer/build -DCMAKE_BUILD_TYPE=Debug
cmake --build ChatServer/build -j2
```

服务端可执行文件输出到：

```text
ChatServer/bin/AIChatServer
```

## 配置和启动

`ChatServer/bin/config.json` 中的 `api_key_env` 是环境变量名，不是 API Key 本身。例如：

```bash
export deepseek_apikey='your-deepseek-key'
export mimo_apikey='your-mimo-key'
export kimi_apikey='your-kimi-key'
```

不要把真实密钥提交到 Git。当前配置中的模型示例包括：

| 模型 | Provider | 当前默认 `max_tokens` |
| --- | --- | ---: |
| `deepseek-flash` | `DeepseekProvider` | `393216` |
| `deepseek-v4-pro` | `DeepseekProvider` | `393216` |
| `mimo-v2.5-pro` | `MimoProvider` | `131072` |
| `kimi-k3` | `KimiProvider` | `393216` |
| `kimi-k2.6` | `KimiProvider` | `393216` |
| `qwen3:0.6b` | `OllamaLLMProvider` | `2048` |

从 `ChatServer/bin` 启动可以保证默认配置、数据库、日志和 `www` 静态目录的相对路径正确：

```bash
cd ChatServer/bin
./AIChatServer --config=./config.json --log_path=./logs
```

常用参数：

```text
--host       监听地址，默认 0.0.0.0
--port       监听端口，默认 8080
--log_path   日志目录、日志文件或 stdout
--log_level  trace/debug/info/warn/error/critical/off
--db_name    SQLite 数据库路径，默认 chat_server.db
--config     Provider 和模型配置路径，默认 ./config.json
```

启动后访问 `http://127.0.0.1:8080/`。如果修改了工作目录，请同时传入绝对路径的 `--config`、`--db_name` 和 `--log_path`，并确保当前目录下存在 `www`，因为服务端会挂载 `./www`。

## API 通用约定

基础地址记为 `BASE_URL`，例如 `http://127.0.0.1:8080`。

成功响应通常为：

```json
{
  "success": true,
  "message": "操作结果",
  "data": {}
}
```

没有返回数据的接口会省略 `data`。接口使用 HTTP 状态码表示失败；多数校验失败响应的正文是错误消息文本，发送验证码过于频繁（429）时会返回带 `data.retry_after` 的 JSON。

登录和注册成功后，服务端会设置：

```text
cookie_id=...; Path=/; HttpOnly; SameSite=Lax; Max-Age=2592000
```

浏览器同源请求会自动携带 Cookie；跨域调用需要使用 `credentials: "include"`。命令行调用可以使用 `curl -c cookies.txt` 保存 Cookie，再使用 `curl -b cookies.txt` 发送 Cookie。

除 `/api/models`、邮箱验证码接口、登录/注册接口和忘记密码接口外，其余接口都应在登录状态下调用。所有 JSON 请求都使用 `Content-Type: application/json`，头像接口是例外，见下文。

## 认证 API

| 方法和路径 | 请求体 | 成功返回 |
| --- | --- | --- |
| `POST /api/auth/email/code` | `{ "email": "a@example.com", "purpose": "register" }` | `data.retry_after` |
| `POST /api/auth/email/verify` | `{ "email": "a@example.com", "code": "123456", "purpose": "register" }` | 无 `data` |
| `POST /api/auth/register` | `{ "email": "a@example.com", "user_name": "alice", "password": "..." }` | `data.user`，同时设置登录 Cookie |
| `POST /api/auth/login` | `{ "email": "a@example.com", "password": "..." }` | 用户对象，同时设置登录 Cookie |
| `POST /api/auth/logout` | 无 | 清除登录 Cookie |
| `GET /api/auth/info` | 无 | 当前用户对象 |
| `POST /api/auth/forget_password` | `{ "email": "a@example.com", "code": "123456", "password": "..." }` | 无 `data` |

用户对象字段为：`user_id`、`user_name`、`email`、`avatar_path`、`create_time`。

示例：

```bash
curl -i -c cookies.txt \
  -H 'Content-Type: application/json' \
  -d '{"email":"a@example.com","password":"your-password"}' \
  http://127.0.0.1:8080/api/auth/login

curl -b cookies.txt http://127.0.0.1:8080/api/auth/info
```

## 用户 API

| 方法和路径 | 请求体 | 说明 |
| --- | --- | --- |
| `POST /api/user/name` | `{ "user_name": "新名称" }` | 修改用户名 |
| `POST /api/user/email` | `{ "email": "new@example.com" }` | 修改邮箱 |
| `POST /api/user/password` | `{ "password": "new-password" }` | 修改密码 |
| `POST /api/user/avatar` | 原始图片二进制 | 修改头像 |

头像接口不能发送 JSON 或 multipart，必须直接发送图片二进制，并且 `Content-Type` 必须精确为 `image/png` 或 `image/jpeg`，大小不能超过 5 MiB：

```bash
curl -b cookies.txt \
  -H 'Content-Type: image/png' \
  --data-binary @avatar.png \
  http://127.0.0.1:8080/api/user/avatar
```

## 模型和会话 API

### 获取可用模型

```http
GET /api/models
```

`data` 是模型数组，每项结构如下：

```json
{
  "name": "deepseek-flash",
  "desc": "DeepSeek Flash",
  "config": {
    "temperature": 0.8,
    "max_tokens": 393216,
    "think": true,
    "reasoning_effort": "high"
  }
}
```

前端应优先使用接口返回的 `config.max_tokens`，不要为所有模型写死同一个上限；例如当前 MiMo 配置为 `131072`，本地 Qwen 配置为 `2048`。

### 创建会话

```http
POST /api/session
```

请求体中的创建字段名是 `model.model_name`：

```json
{
  "model": {
    "model_name": "deepseek-flash",
    "temperature": 0.8,
    "max_tokens": 393216,
    "think": true,
    "reasoning_effort": "high"
  },
  "session_name": "新会话"
}
```

成功时 `data` 包含 `session_id`、`session_name` 和实际生效的 `model` 配置。

### 会话列表和历史

| 方法和路径 | 返回 |
| --- | --- |
| `GET /api/sessions` | 会话数组，含 `session_id`、名称、模型、时间、消息数、`model_config`、`first_message` |
| `GET /api/session/{session_id}/history` | 消息数组，每项含 `role`、`content`、`timestamp`、`message_id` |
| `DELETE /api/session/{session_id}` | 删除单个会话 |
| `DELETE /api/sessions` | 删除当前用户的全部会话 |

时间字段是 Unix 秒时间戳。

### 修改会话

修改会话名称：

```json
POST /api/session/name
{"session_id":"session-id","session_name":"工作对话"}
```

切换模型：

```json
POST /api/session/model
{"session_id":"session-id","model":"mimo-v2.5-pro"}
```

修改模型参数：

```json
POST /api/session/model_config
{
  "session_id": "session-id",
  "model": "mimo-v2.5-pro",
  "temperature": 0.8,
  "max_tokens": 131072,
  "think": true,
  "reasoning_effort": "high"
}
```

注意：`/api/session` 创建会话时使用 `model.model_name`，而两个修改接口使用顶层字段 `model`。

## 消息 API

### 非流式对话

```http
POST /api/message
```

请求：

```json
{"session_id":"session-id","message":"你好"}
```

成功响应的 `data`：

```json
{"response":"你好，有什么可以帮你？","session_id":"session-id"}
```

### SSE 流式对话

```http
POST /api/message/async
Content-Type: application/json
Accept: text/event-stream
```

请求体与非流式接口相同。服务端返回 `text/event-stream`，每个事件格式为：

```text
data: "{\"success\":true,\"message\":\"send message success\",\"data\":{\"response\":\"文本片段\",\"session_id\":\"session-id\"}}"

data: [DONE]

```

事件中的 JSON 作为 SSE `data` 字符串再次进行了 JSON 转义，客户端应先去掉 `data: ` 和空行，再解析这一层字符串；收到 `[DONE]` 后结束读取。

示例：

```bash
curl -N -b cookies.txt \
  -H 'Content-Type: application/json' \
  -H 'Accept: text/event-stream' \
  -d '{"session_id":"session-id","message":"请介绍一下这个项目"}' \
  http://127.0.0.1:8080/api/message/async
```

## 常见问题

1. **服务启动后页面空白**：确认进程工作目录是 `ChatServer/bin`，并检查 `ChatServer/bin/www` 是否存在。
2. **ChatServer 构建时找不到 SDK**：先执行 `sudo cmake --install SDK/build`，确认 `/usr/local/include/ai_chat_sdk` 和 `/usr/local/lib/libai_chat_sdk.a` 存在。
3. **接口突然返回 401**：检查 Cookie 是否被客户端保存和发送；浏览器跨域请求必须设置 `credentials: "include"`。
4. **模型列表为空或初始化失败**：检查 `config.json` 的 Provider 名称、模型名称和 API Key 环境变量是否正确；Ollama 模型还需要确认 `http://127.0.0.1:11434` 可访问。
5. **流式响应不更新**：客户端需要按 SSE 事件读取，使用 `curl` 调试时加 `-N`，不要等待完整 JSON 响应后再渲染。

## 版本信息

服务端版本号：`2.0.0`。
