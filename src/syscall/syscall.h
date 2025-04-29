
#if !defined(SYSCALL_H)
#define SYSCALL_H

#include <stdint.h>

#define ONE_MS 1000000LL
#define ONE_SC 1000000000LL

extern void
safe_nanosleep(long ns);

extern int64_t
safe_rand(void);

#endif /* !defined(SYSCALL_H) */
