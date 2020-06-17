#ifndef __SERVER__
#define __SERVER__

#include <iostream>

#define MAX_PATH 256

#define SAFE_DELETE(x) \
    if (x != NULL) delete (x);

#endif  //__SERVER__