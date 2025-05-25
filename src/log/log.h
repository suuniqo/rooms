
#if !defined(LOG_H)
#define LOG_H

extern void
log_init(const char* user, const char* mode);

extern void
log_error(const char* fmt, ...);

extern void
log_shutdown(const char* fmt, ...);

#endif /* !defined(LOG_H) */
