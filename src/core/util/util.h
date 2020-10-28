#ifndef __KIM_UTIL_H__
#define __KIM_UTIL_H__

#include <string>
#include <vector>

std::vector<std::string> split_str(const std::string& s, char delim);
std::string format_addr(const std::string& host, int port);
std::string format_identity(const std::string& host, int port, int index);
std::string format_str(const char* const fmt, ...);
std::string work_path();
std::string format_redis_cmds(const std::vector<std::string>& argv);
std::string md5(const std::string& data);

#ifdef __cplusplus
extern "C" {
#endif

void daemonize();
bool adjust_files_limit(int& max_clients);
const char* to_lower(const char* s, int len);
long long mstime();     // millisecond
long long ustime();     // microseconds
double time_now();      // seconds (double)
double decimal_rand();  // [0, 1.0) double random.

#ifdef __cplusplus
}
#endif

#endif  //__KIM_UTIL_H__