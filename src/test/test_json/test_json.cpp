#include <iostream>
#include <vector>

#include "util/json/CJsonObject.hpp"

kim::CJsonObject config;

typedef struct db_info_s {
    int port = 0;
    int max_conn_cnt = 0;
    std::string host, db_name, password, charset, user;
} db_info_t;

void test_parse(kim::CJsonObject& config) {
    kim::CJsonObject in, out;
    in.Add("path", "/kimserver/gate/kimserver-gate000000001");
    in.Add("type", "gate");
    in.Add("ip", "127.0.0.1");
    in.Add("port", 12345);
    in.Add("worker_cnt", config("worker_cnt"));

    if (out.Parse(in.ToString())) {
        std::cout << "parse json done!" << std::endl
                  << out.ToString() << std::endl;
    } else {
        std::cout << "parse json failed!" << std::endl;
    }
}

void test_children(kim::CJsonObject& config) {
    std::string node("database");
    /* get keys. */
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
        info->port = std::stoi(obj("port"));
        std::cout << info->host << " "
                  << info->port << " "
                  << info->db_name << " "
                  << info->password << " "
                  << info->charset << " "
                  << info->user << " "
                  << std::endl;
        delete info;
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

    std::cout << "--------" << std::endl;
    std::cout << config.ToFormattedString() << std::endl;
}

void test_array(kim::CJsonObject& config) {
    /* printf array. */
    std::cout << "--------" << std::endl;
    // std::cout << config.ToString() << std::endl;
    std::cout << config["zookeeper"]["subscribe_node_type"].ToFormattedString() << std::endl;
    std::cout << config["zookeeper"]["subscribe_node_type"].IsArray() << std::endl;

    kim::CJsonObject& o = config["zookeeper"]["subscribe_node_type"];
    for (int i = 0; i < o.GetArraySize(); i++) {
        std::cout << o(i) << std::endl;
    }

    /* add array */
    std::cout << "--------" << std::endl;
    kim::CJsonObject test, node;
    test.Add("nodes", kim::CJsonObject("[]"));
    node.Add("ip", "234");
    node.Add("port", 38749);
    test["nodes"].Add(node);

    node.Clear();
    node.Add("ip", "845748");
    node.Add("port", 334);
    test["nodes"].Add(node);
    std::cout << test.ToFormattedString() << std::endl;
}

int main() {
    if (!config.Load("../../../bin/config.json")) {
        std::cerr << "load json config failed!" << std::endl;
        return 1;
    }

    // test_parse(config);
    // test_children(config);
    test_array(config);
    return 0;
}