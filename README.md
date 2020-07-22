# kimserver

Kimserver is an async tcp multiple processes (one master process and one or more worker processes) high performance server.

> **resources**:  [Nebula](https://github.com/Bwar/Nebula) / [thunder](https://github.com/doerjiayi/thunder) / [redis](https://github.com/antirez/redis) / [nginx](https://github.com/nginx/nginx) / [http-parser](https://github.com/nodejs/http-parser).

---

## 1. environment

* enable c++11.
* install g++ / libev / protobuf / cryptopp.

---

## 2. work

work on Linux / MacOS

```shell
./run.sh
```

---

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

## 3. usage

### 3.1. test

* request

```shell
curl -v -d '{"uid":"hello world"}' http://127.0.0.1:3355/kim/im/user/
```

* response

```json
{
    "code": 0,
    "msg": "error"
}
```

---

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
    LOG_DEBUG("delete cmd hello");
}

Cmd::STATUS CmdHello::call_back(std::shared_ptr<Request> req) {
    const HttpMsg* msg = req->get_http_msg();
    if (msg == nullptr) {
        return Cmd::STATUS::ERROR;
    }

    LOG_DEBUG("cmd hello, http path: %s", msg->path().c_str());

    HttpMsg m;
    m.set_type(HTTP_RESPONSE);
    m.set_status_code(200);
    m.set_http_major(msg->http_major());
    m.set_http_minor(msg->http_minor());

    CJsonObject obj;
    obj.Add("code", 0);
    obj.Add("msg", "error");
    m.set_body(obj.ToFormattedString());
    return response(m);
}

}
```
