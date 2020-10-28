#include <iostream>
#include <vector>

#include "util/json/CJsonObject.hpp"

typedef struct db_info_s {
    int port = 0;
    int max_conn_cnt = 0;
    std::string host, db_name, password, charset, user;
} db_info_t;

int main() {
    kim::CJsonObject config;
    if (!config.Load("../../../bin/config.json")) {
        std::cerr << "load json config failed!" << std::endl;
        return 1;
    }

    std::cout << config.ToString() << std::endl;
    // std::cout << config.ToFormattedString() << std::endl;

    std::string node("database");

    if (config[node].IsEmpty()) {
        std::cerr << "can not find key" << std::endl;
        return 1;
    }

    std::cout << std::endl
              << config[node].ToString()
              << std::endl
              << config[node]["test"].ToString()
              << std::endl
              << config[node]["test"]("host")
              << std::endl
              << config[node]["test"]("xxxxxxx")
              << std::endl;

    // get keys
    std::vector<std::string> vec;
    config[node].GetKeys(vec);
    for (const auto& it : vec) {
        std::cout << it << std::endl;
        const kim::CJsonObject& obj = config[node][it];
        db_info_t* info = new db_info_t;
        info->host = obj("host");
        info->db_name = obj("name").empty() ? "mysql" : obj("name");
        info->password = obj("password");
        info->charset = obj("charset");
        info->user = obj("user");
        info->port = atoi(obj("port").c_str());
        std::cout << info->host << " "
                  << info->port << " "
                  << info->db_name << " "
                  << info->password << " "
                  << info->charset << " "
                  << info->user << " "
                  << std::endl;
        delete info;
    }

    std::cout << "--------" << std::endl;
    std::cout << config["zookeeper"]["subscribe"].ToFormattedString() << std::endl;
    std::cout << config["zookeeper"]["subscribe"].IsArray() << std::endl;

    kim::CJsonObject& o = config["zookeeper"]["subscribe"];
    for (int i = 0; i < o.GetArraySize(); i++) {
        std::cout << o(i) << std::endl;
    }

    std::cout << "--------" << std::endl;
    std::cout << config.ToFormattedString() << std::endl;
    return 0;
}