#ifndef __SERVER__
#define __SERVER__

#include "log.h"

#define LL_EMERG 0   /* system is unusable */
#define LL_ALERT 1   /* action must be taken immediately */
#define LL_CRIT 2    /* critical conditions */
#define LL_ERR 3     /* error conditions */
#define LL_WARNING 4 /* warning conditions */
#define LL_NOTICE 5  /* normal but significant condition */
#define LL_INFO 6    /* informational */
#define LL_DEBUG 7   /* debug-level messages */

#define SAFE_DELETE(x) \
    if (x != NULL) delete (x);

kim::Log* g_log = NULL;

#define LOG_DETAIL(level, args...)                                        \
    if (g_log != NULL) {                                                  \
        g_log->log_data(__FILE__, __LINE__, __FUNCTION__, level, ##args); \
    }

#define LOG_INFO(args...) LOG_DETAIL(LL_INFO, ##args)

#endif  //__SERVER__