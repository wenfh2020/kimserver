#ifndef __UTIL_H__
#define __UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

// namespace kim {

void daemonize();
bool adjust_files_limit(int& max_clients);
const char* to_lower(const char* s, int len);
long long mstime();  // millisecond
long long ustime();  // microseconds

// }  // namespace kim

#ifdef __cplusplus
}
#endif

#endif  //__UTIL_H__