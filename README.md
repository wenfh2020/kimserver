# kimserver

Kimserver is an async tcp multiple processes (one master process and one or more worker processes) high performance server.

> **resources**:  [Nebula](https://github.com/Bwar/Nebula) / [thunder](https://github.com/doerjiayi/thunder) / [redis](https://github.com/antirez/redis) / [nginx](https://github.com/nginx/nginx) / [http-parser](https://github.com/nodejs/http-parser).

---
<!-- TOC -->

1. [1. environment](#1-environment)
2. [2. work](#2-work)
    1. [2.1. config](#21-config)
3. [3. usage](#3-usage)
    1. [3.1. test](#31-test)
    2. [3.2. module](#32-module)
    3. [3.3. cmd](#33-cmd)

<!-- /TOC -->
<a id="markdown-1-environment" name="1-environment"></a>
## 1. environment

* enable c++11.
* install g++ / libev / protobuf / cryptopp.

---

<a id="markdown-2-work" name="2-work"></a>
## 2. work

work on Linux / MacOS

```shell
./run.sh
```

---

<a id="markdown-21-config" name="21-config"></a>
### 2.1. config

```json
{
    "worker_processes": 4,
    "node_type": "gate",
    "server_name": "kimserver",
    "bind": "127.0.0.1",
    "port": 3344,
    "gate_bind": "127.0.0.1",
    "gate_port": 3355,
    "gate_codec": 1,
    "keep_alive": 30,
    "log_path": "kimserver.log",
    "log_level": "debug"
}
```

| name             | desc                                                                                              |
| :--------------- | :------------------------------------------------------------------------------------------------ |
| worker_processes | child process's number.                                                                           |
| node_type        | cluster node type.                                                                                |
| server_name      | server manager process's name, and child process name format: name_w_number, like: kimserver_w_1. |
| bind             | host for cluster inner contact.                                                                   |
| port             | bind port.                                                                                        |
| gate_bind        | host for user's client contact.                                                                   |
| gate_port        | gate_bind's port.                                                                                 |
| gate_codec       | protobuf:1, http: 2, private: 3.                                                                  |
| keep_alive       | connection keep alive time (seconds).                                                             |
| log_path         | log file path.                                                                                    |
| log_level        | log level: debug, info, notice, warning, err, crit, alert, emerg.                                 |

---

<a id="markdown-3-usage" name="3-usage"></a>
## 3. usage

<a id="markdown-31-test" name="31-test"></a>
### 3.1. test

* request

```shell
curl -v -d '{"uid":"hello world"}' http://127.0.0.1:3355/kim/im/user/ | python -m json.tool
```

* response

```json
{
    "code": 0,
    "msg": "success",
    "data": {
        "id": "123",
        "name": "kimserver"
    }
}
```

---

<a id="markdown-32-module" name="32-module"></a>
### 3.2. module

* module_core.h

```c++
namespace kim {

class MoudleCore : public Module {
    BEGIN_HTTP_MAP()
    HTTP_HANDLER("/kim/im/user/", CmdHello, "hello");
    END_HTTP_MAP()
};

}  // namespace kim
```

---

<a id="markdown-33-cmd" name="33-cmd"></a>
### 3.3. cmd

* cmd_hello.h

```c++
namespace kim {

class CmdHello : public Cmd {
   public:
    CmdHello() {}
    virtual ~CmdHello();
    virtual Cmd::STATUS call_back(std::shared_ptr<Request> req);
};

}  // namespace kim
```

* cmd_hello.cpp

```c++

namespace kim {

CmdHello::~CmdHello() {
    LOG_DEBUG("~CmdHello()");
}

Cmd::STATUS CmdHello::call_back(std::shared_ptr<Request> req) {
    const HttpMsg* msg = req->get_http_msg();
    if (msg == nullptr) {
        return Cmd::STATUS::ERROR;
    }

    LOG_DEBUG("cmd hello, http path: %s, data: %s",
              msg->path().c_str(), msg->body().c_str());

    CJsonObject data;
    data.Add("id", "123");
    data.Add("name", "kimserver");

    CJsonObject obj;
    obj.Add("code", 0);
    obj.Add("msg", "success");
    obj.Add("data", data);
    return response_http(obj.ToString());
}

}
```
