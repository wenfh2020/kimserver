#include <mysql/mysql.h>
#include <sys/time.h>

#include <iostream>

int g_test_cnt = 0;
int g_port = 3306;
bool g_is_write = false;
const char *g_db = "mysql";
const char *g_user = "root";
const char *g_password = "root123!@#";
const char *g_host = "127.0.0.1";

/*

g++ sync.cpp -lmysqlclient -o sync && ./sync read 10000

CREATE DATABASE IF NOT EXISTS mytest DEFAULT CHARSET utf8mb4 COLLATE utf8mb4_general_ci;

CREATE TABLE `test_async_mysql` (
    `id` int(11) unsigned NOT NULL AUTO_INCREMENT,
    `value` varchar(32) NOT NULL,
    PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=1 DEFAULT CHARSET=utf8mb4; 
*/

double time_now() {
    struct timeval tv;
    gettimeofday(&tv, 0);
    return ((tv).tv_sec + (tv).tv_usec * 1e-6);
}

bool check_args(int args, char **argv) {
    if (args < 2) {
        std::cerr << "invalid args! exp: './amysql read 1' or './amysql write 1'"
                  << std::endl;
        return false;
    }

    if (!strcasecmp(argv[1], "write")) {
        g_is_write = true;
    }

    if (argv[2] == nullptr || !isdigit(argv[2][0]) || atoi(argv[2]) == 0) {
        std::cerr << "invalid test cnt." << std::endl;
        return false;
    }

    g_test_cnt = atoi(argv[2]);
    return true;
}

void show_error(MYSQL *mysql) {
    printf("error: %d, errstr: %s\n", mysql_errno(mysql), mysql_error(mysql));
}

int main(int args, char **argv) {
    if (!check_args(args, argv)) {
        return 1;
    }

    MYSQL *mysql;
    MYSQL_RES *result;
    const char *query;

    mysql = mysql_init(NULL);
    if (!mysql_real_connect(mysql, g_host, g_user, g_password, g_db, g_port, NULL, 0)) {
        show_error(mysql);
        return 1;
    }

    double begin_time = time_now();
    for (int i = 0; i < g_test_cnt; i++) {
        if (g_is_write) {
            query = "insert into mytest.test_async_mysql (value) values ('12345566');";
            if (mysql_real_query(mysql, query, strlen(query))) {
                show_error(mysql);
            }
        } else {
            query = "select value from mytest.test_async_mysql where id = 1;";
            if (mysql_real_query(mysql, query, strlen(query))) {
                show_error(mysql);
            }
            result = mysql_store_result(mysql);
            printf("query rows: %lu\n",
                   (unsigned long)mysql_affected_rows(mysql));
            mysql_free_result(result);
        }
    }

    mysql_close(mysql);
    std::cout << "spend time: " << time_now() - begin_time << std::endl
              << "oper avg:   " << g_test_cnt / (time_now() - begin_time)
              << std::endl;
    return 0;
}