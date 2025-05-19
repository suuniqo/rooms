
#if !defined(ERROR_H)
#define ERROR_H

extern void
error_init(const char* user, const char* mode);

extern void
error_log(const char* fmt, ...);

extern void
error_shutdown(const char* fmt, ...);

#endif /* !defined(ERROR_H) */
