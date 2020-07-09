#ifndef __UTIL_H__
#define __UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

void daemonize();
const char* to_lower(const char* s, int len);

#ifdef __cplusplus
}
#endif

#endif  //__UTIL_H__