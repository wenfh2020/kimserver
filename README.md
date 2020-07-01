# kimserver

Kimserver is an async c++ multiple processes (one master process and one or more worker processes) high performance server.

> which learn from [Nebula](https://github.com/Bwar/Nebula) / [thunder](https://github.com/doerjiayi/thunder) and [redis](https://github.com/antirez/redis).

---

## 1. environment

* install g++.
* install `libev`.
* enable c++11.

---

## 2. work

work on Linux / Macos

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
    "log_path": "kimserver.log",
    "log_level": "debug"
}
```

| name             | desc                                                                                              |
| :--------------- | :------------------------------------------------------------------------------------------------ |
| worker_processes | child process's number.                                                                           |
| node_type        | cluster node type.                                                                                |
| server_name      | server manager process's name, and child process name format: name_w_number, like: kimserver_w_1. |
| bind             | listen host for cluster inner contact.                                                            |
| port             | bind port.                                                                                        |
| gate_bind        | hosts for user's client contact.                                                                  |
| gate_port        | gate_bind's port.                                                                                 |
| log_path         | log file path.                                                                                    |
| log_level        | log level: debug, info, notice, warning, err, crit, alert, emerg.                                 |
