#ifndef __UTIL_H__
#define __UTIL_H__

#include <string>
#include <vector>

std::vector<std::string> split_str(const std::string& s, char delim);
std::string format_addr(const std::string& host, int port);
std::string format_str(const char* const fmt, ...);
std::string get_work_path();
std::string format_redis_cmds(const std::vector<std::string>& cmd_argv);

#ifdef __cplusplus
extern "C" {
#endif

// namespace kim {

void daemonize();
bool adjust_files_limit(int& max_clients);
const char* to_lower(const char* s, int len);
long long mstime();     // millisecond
long long ustime();     // microseconds
double time_now();      // seconds (double)
double decimal_rand();  // [0, 1.0) double random.

// }  // namespace kim

#ifdef __cplusplus
}
#endif

#endif  //__UTIL_H__