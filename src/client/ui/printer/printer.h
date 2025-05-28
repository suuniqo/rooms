
#if !defined(PRINTER_H)
#define PRINTER_H

typedef struct printer printer_t;

extern void
printer_init(printer_t** printer);

extern void
printer_append(printer_t* printer, const char* fmt, ...);

extern void
printer_dump(printer_t* printer);

#endif /* !defined(PRINTER_H) */
