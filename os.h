#ifndef OS_H
#define OS_H

#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

extern ssize_t (*os_read)(int fd, void *dest, size_t count);
extern ssize_t (*os_write)(int fd, const void *buf, size_t count);

void os_abort(void);

#ifdef __cplusplus
}
#endif

#endif /* !OS_H */
