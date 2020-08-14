# kimserver

Kimserver is an async tcp multiple processes (one master process and one or more worker processes) high performance server.

> **resources**:  [Nebula](https://github.com/Bwar/Nebula) / [thunder](https://github.com/doerjiayi/thunder) / [redis](https://github.com/antirez/redis) / [hiredis](https://github.com/redis/hiredis) / [nginx](https://github.com/nginx/nginx) / [http-parser](https://github.com/nodejs/http-parser) / [jemalloc](https://github.com/jemalloc/jemalloc) .

---
<!-- TOC -->

1. [1. environment](#1-environment)
2. [2. work](#2-work)
    1. [2.1. config](#21-config)
3. [3. usage](#3-usage)
    1. [3.1. test](#31-test)
    2. [3.2. module](#32-module)

<!-- /TOC -->
<a id="markdown-1-environment" name="1-environment"></a>
## 1. environment

enable c++11 and install 3rd lib.

| 3rd      | version |
| :------- | :------ |
| g++      | 4.2.1   |
| libev    | 4.27    |
| protobuf | 4.0.0   |
| cryptopp | 8.1.0   |
| hiredis  | 0.14.0  |
| jemalloc | 5.2.1   |

> **[note]** install protobuf use script: [install_protobuf.sh](https://github.com/wenfh2020/kimserver/blob/master/script/install/install_protobuf.sh)

---

<a id="markdown-2-work" name="2-work"></a>
## 2. work

run on Linux / MacOS

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
    "log_level": "debug",
    "modules": [
        "module_test.so"
    ],
    "redis": {
        "test": {
            "host": "127.0.0.1",
            "port": 6379
        }
    }
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
curl -v -d '{"uid":"hello world"}' http://127.0.0.1:3355/kim/helloworld/ | python -m json.tool
```

* response

```json
{
    "code": 0,
    "msg": "ok",
    "data": {
        "id": "123",
        "name": "kimserver"
    }
}
```

---

<a id="markdown-32-module" name="32-module"></a>
### 3.2. module

```c++
namespace kim {

class MoudleTest : public Module {
    REGISTER_HANDLER(MoudleTest)

   public:
    void register_handle_func() {
        REGISTER_HTTP_FUNC("/kim/test/", MoudleTest::func_test_cmd);
        REGISTER_HTTP_FUNC("/kim/helloworld/", MoudleTest::func_hello_world);
    }

   private:
    Cmd::STATUS func_test_cmd(std::shared_ptr<Request> req) {
        // cmd for async/sync logic.
        HANDLE_CMD(CmdHello);
    }

    Cmd::STATUS func_hello_world(std::shared_ptr<Request> req) {
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
        obj.Add("msg", "ok");
        obj.Add("data", data);
        return response_http(req->get_conn(), obj.ToString());
    }
};

}  // namespace kim
```
