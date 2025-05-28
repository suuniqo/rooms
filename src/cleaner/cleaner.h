
#if !defined(CLEANER_H)
#define CLEANER_H

typedef void (*cleaner_fn_t)(void*);
typedef struct cleaner cleaner_t;

extern void
cleaner_init(void);

extern void
cleaner_push(cleaner_fn_t cleaner_fn, void** arg);

extern void
cleaner_run(void);

#endif /* CLEANER_H */
