# Kimserver

Kimserver is an async tcp multiple processes (master & workers) high-performance server.

> **resources**:  [Nebula](https://github.com/Bwar/Nebula) / [thunder](https://github.com/doerjiayi/thunder) / [redis](https://github.com/antirez/redis) / [hiredis](https://github.com/redis/hiredis) / [nginx](https://github.com/nginx/nginx) / [http-parser](https://github.com/nodejs/http-parser) / [jemalloc](https://github.com/jemalloc/jemalloc) .

---
<!-- TOC -->

- [Kimserver](#kimserver)
  - [1. Environment](#1-environment)
  - [2. Work](#2-work)
    - [2.1. Config](#21-config)
  - [3. Usage](#3-usage)
    - [3.1. Test](#31-test)
    - [3.2. Module](#32-module)

<!-- /TOC -->
<a id="markdown-1-environment" name="1-environment"></a>
## 1. Environment

Enable C++11 and install third party libs.

| 3rd                                                                                                            | version |
| :------------------------------------------------------------------------------------------------------------- | :------ |
| g++                                                                                                            | 4.2.1   |
| libev                                                                                                          | 4.27    |
| protobuf                                                                                                       | 4.0.0   |
| cryptopp                                                                                                       | 8.1.0   |
| hiredis                                                                                                        | 0.14.0  |
| jemalloc                                                                                                       | 5.2.1   |
| [mariadb-connector-c](http://mariadb.mirror.iweb.com//connector-c-3.1.9/mariadb-connector-c-3.1.9-src.tar.gz ) | 3.1.9   |

> **[note]** Install protobuf use script: [install_protobuf.sh](https://github.com/wenfh2020/kimserver/blob/master/script/install/install_protobuf.sh)

---

<a id="markdown-2-work" name="2-work"></a>
## 2. Work

Run on Linux / MacOS

```shell
./run.sh
```

---

<a id="markdown-21-config" name="21-config"></a>
### 2.1. Config

```json
{
    "worker_cnt": 4,
    "node_type": "gate",
    "server_name": "kimserver",
    "node_host": "127.0.0.1",
    "node_port": 3344,
    "gate_host": "127.0.0.1",
    "gate_port": 3355,
    "gate_codec": "http",
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
    },
    "database": {
        "test": {
            "host": "127.0.0.1",
            "port": 3306,
            "user": "root",
            "password": "root123!@#",
            "charset": "utf8mb4",
            "max_conn_cnt": 1
        }
    }
}
```

| name        | desc                                                                                              |
| :---------- | :------------------------------------------------------------------------------------------------ |
| worker_cnt  | child process's number.                                                                           |
| node_type   | cluster node type.                                                                                |
| server_name | server manager process's name, and child process name format: name_w_number, like: kimserver_w_1. |
| node_host   | host for cluster inner contact.                                                                   |
| node_port   | node  port.                                                                                       |
| gate_host   | host for user's client contact.                                                                   |
| gate_port   | gate server port.                                                                                 |
| gate_codec  | "protobuf", "http".                                                                               |
| keep_alive  | connection keep alive time (seconds).                                                             |
| log_path    | log file. path.                                                                                   |
| log_level   | log level: debug, info, notice, warning, err, crit, alert, emerg.                                 |
| modules     | protocol route container, work as so.                                                             |
| redis       | redis addr config.                                                                                |
| database    | database (mysql) info.                                                                            |

---

<a id="markdown-3-usage" name="3-usage"></a>
## 3. Usage

<a id="markdown-31-test" name="31-test"></a>
### 3.1. Test

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
### 3.2. Module

[demo.](https://github.com/wenfh2020/kimserver/blob/master/src/modules/module_test/module_test.h)

```c++
namespace kim {

class MoudleTest : public Module {
    REGISTER_HANDLER(MoudleTest)

   public:
    void register_handle_func() {
        HANDLE_HTTP_FUNC("/kim/test/", MoudleTest::func_test_cmd);
        HANDLE_HTTP_FUNC("/kim/helloworld/", MoudleTest::func_hello_world);
    }

   private:
    Cmd::STATUS func_test_cmd(std::shared_ptr<Request> req) {
        // cmd for async/sync logic.
        HANDLE_CMD(CmdHello);
    }

    Cmd::STATUS func_hello_world(std::shared_ptr<Request> req) {
        const HttpMsg* msg = req->http_msg();
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
        return response_http(req->conn(), obj.ToString());
    }
};

}  // namespace kim
```
