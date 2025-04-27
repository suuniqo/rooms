
#if !defined(CLEANER_H)
#define CLEANER_H

typedef void (*cleaner_fn_t)(void*);
typedef struct cleaner cleaner_t;

extern void
cleaner_init(cleaner_t** cleaner);

extern void
cleaner_free(cleaner_t* cleaner);

extern void
cleaner_push(cleaner_t* cleaner, cleaner_fn_t cleaner_fn, void* arg);

extern void
cleaner_run(cleaner_t* cleaner);

#endif /* CLEANER_H */
