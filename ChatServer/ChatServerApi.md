---
title: 默认模块
language_tabs:
  - shell: Shell
  - http: HTTP
  - javascript: JavaScript
  - ruby: Ruby
  - python: Python
  - php: PHP
  - java: Java
  - go: Go
toc_footers: []
includes: []
search: true
code_clipboard: true
highlight_theme: darkula
headingLevel: 2
generator: "@tarslib/widdershins v4.0.30"

---

# 默认模块

Base URLs:

# Authentication

# ChatServer

## POST CreateNewSession

POST /api/session

> Body 请求参数

```json
{
    "model": "deepseek-flash"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|Content-Type|header|string| 否 |none|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{
    "data": {
        "model": {
            "max_tokens": 2048,
            "model": "deepseek-flash",
            "reasoning_effort": "high",
            "temperature": 0.80000000000000004,
            "think": true
        },
        "session_id": "session_1790078926_00000003",
        "session_name": "new session"
    },
    "message": "create session success",
    "success": true
}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

## DELETE DeleteSession

DELETE /api/session/session_1790068507_00000001

> 返回示例

> 200 Response

```json
{
	"message" : "delete session success",
	"success" : true
}

```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

## GET GetSessionHistory

GET /api/session/session_1790068708_00000002/history

> 返回示例

> 200 Response

```json
{"data":null,"message":"get session history success","success":true}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

## GET GetModels

GET /api/models

> 返回示例

> 200 Response

```json
{"data":[{"desc":"DeepSeek Flash","name":"deepseek-flash"},{"desc":"Kimi K2.6","name":"kimi-k2.6"},{"desc":"Kimi K3","name":"kimi-k3"},{"desc":"MiMo v2.5 Pro","name":"mimo-v2.5-pro"},{"desc":"Local Qwen3 0.6B model","name":"qwen3:0.6b"}],"message":"get models success","success":true}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

## POST ChangeModelConfig

POST /api/session/model_config

> Body 请求参数

```json
{
    "session_id": "session_1790068708_00000002",
    "model": "deepseek-flash",
    "temperature": 0.9,
    "max_tokens": 1024,
    "think": true,
    "reasoning_effort": "high"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{"message":"change model config success","success":true}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

## POST ChangeModel

POST /api/session/model

> Body 请求参数

```json
{
    "session_id": "session_1790068708_00000002",
    "model": "kimi-k3"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{"message":"change model success","success":true}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

## GET GetSessionList

GET /api/sessions

> 返回示例

> 200 Response

```json
{"data":[{"first_message":"new session","message_count":0,"model":"kimi-k3","session_id":"session_1790068708_00000002","start_time":1790068708,"update_time":1790068708}],"message":"get session list success","success":true}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

## POST SendMessage

POST /api/message

> Body 请求参数

```json
{
    "session_id": "session_1790068708_00000002",
    "message": "你好"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{"data":{"response":"\u4f60\u597d\uff01\u5f88\u9ad8\u5174\u89c1\u5230\u4f60\u3002\ud83d\ude0a\n\n\u6709\u4ec0\u4e48\u6211\u53ef\u4ee5\u5e2e\u52a9\u4f60\u7684\u5417\uff1f\u65e0\u8bba\u662f\u56de\u7b54\u95ee\u9898\u3001\u5199\u4f5c\u3001\u7ffb\u8bd1\uff0c\u8fd8\u662f\u804a\u804a\u5929\uff0c\u6211\u90fd\u5f88\u4e50\u610f\u5e2e\u5fd9\u3002","session_id":"session_1790068708_00000002"},"message":"send message success","success":true}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

## POST SendMessageStream

POST /api/message/async

> Body 请求参数

```json
{
    "session_id": "session_1790068708_00000002",
    "message": "你是什么模型"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

```json
data: "{\"data\":{\"response\":\"\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\我\是\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

...

data: [DONE]

```

```json
data: "{\"data\":{\"response\":\"\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\我\是\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\" **\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"K\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"imi\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"**\，\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\由\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\" **\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\月\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\之\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\暗\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\面\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\（\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"Moon\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"shot\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\" AI\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\）\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"**\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\" \",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\开\发\的\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\" AI\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\" \",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\助\手\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\。\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\具\体\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\模\型\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\版\本\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\和\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\实\现\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\细\节\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\我\这\里\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\无\法\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\提\供\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\。\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\有\什\么\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\我\可\以\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\帮\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\你\的\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\吗\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\？\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: "{\"data\":{\"response\":\"\",\"session_id\":\"session_1790068708_00000002\"},\"message\":\"send message success\",\"success\":true}"

data: [DONE]

```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

## POST ChangeSessionName

POST /api/session/name

> Body 请求参数

```json
{
    "session_id": "session_1790079529_00000001",
    "session_name": "old session"
}
```

### 请求参数

|名称|位置|类型|必选|说明|
|---|---|---|---|---|
|body|body|object| 是 |none|

> 返回示例

> 200 Response

```json
{}
```

### 返回结果

|状态码|状态码含义|说明|数据模型|
|---|---|---|---|
|200|[OK](https://tools.ietf.org/html/rfc7231#section-6.3.1)|none|Inline|

### 返回数据结构

# 数据模型

