
#if !defined(UIFMT_H)
#define UIFMT_H

/*** defines ***/

 /* font */
#define THIN 0
#define BOLD 1
#define FAINT 2
#define ITALIC 3
#define UNDERLINED 4
#define INVERTED 7

#define FONT_FORMAT_UNWRAP(COLOR, BOLD) "\x1b[" #BOLD ";38;5;" #COLOR "m"
#define FONT_FORMAT(COLOR, BOLD) FONT_FORMAT_UNWRAP(COLOR, BOLD)

#define FONT_STYLE_UNWRAP(COLOR, STYLE, BOLD) "\x1b[" #BOLD ";" #STYLE ";38;5;" #COLOR "m"
#define FONT_STYLE(COLOR, STYLE, BOLD) FONT_STYLE_UNWRAP(COLOR, STYLE, BOLD)

#define FONT_BOLD "\x1b[1m"
#define FONT_RESET "\x1b[0m"

 /* color */
#define COLOR_LIGHT 146
#define COLOR_DEFAULT 103
#define COLOR_DARK 60

 /* cursor */
#define CURSOR_RIGHT_UNWRAP(N) "\x1b[" #N "C"
#define CURSOR_MOVE_UNWRAP(ROW, COL) "\x1b[" #ROW ";" #COL "H"

#define CURSOR_MOVE(ROW, COL) CURSOR_MOVE_UNWRAP(ROW, COL)
#define CURSOR_RIGHT(N) CURSOR_RIGHT_UNWRAP(N) 

#define CURSOR_RESET "\x1b[H"
#define CURSOR_SHOW "\x1b[?25h"
#define CURSOR_HIDE "\x1b[?25l"

 /* screen*/
#define CLEAR_SCREEN "\x1b[2J"
#define CLEAR_RIGHT "\x1b[K"
#define CLEAR_LEFT "\x1b[1K"
#define CLEAR_TO_END "\x1b[0J"
#define CLEAR_FROM_START "\x1b[1J"

#define RESET_SCROLL "\x1b[r"

#define ENABLE_SCROLL_EVENT "\x1b[?1003h"
#define DISABLE_SCROLL_EVENT "\x1b[?1003l"

#define INIT_ALT_BUF "\x1b[?1049h"
#define KILL_ALT_BUF "\x1b[?1049l"

#define INIT_BRACKETED_PASTE "\x1b[?2004h"
#define KILL_BRACKETED_PASTE "\x1b[?2004l"

#endif /* !defined(UIFMT_H) */
