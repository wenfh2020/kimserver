#ifndef __SERVER__
#define __SERVER__

#define SAFE_DELETE(x) \
    if (x != NULL) delete (x);

#endif  //__SERVER__