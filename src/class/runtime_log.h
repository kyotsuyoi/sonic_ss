#ifndef RUNTIME_LOG_H
#define RUNTIME_LOG_H

#include <stdbool.h>

typedef enum
{
	RuntimeLogModeOff = 0,
	RuntimeLogModeSystem,
	RuntimeLogModeSystemVerbose,
	RuntimeLogModeSprite,
	RuntimeLogModeCount
} runtime_log_mode_t;

// Logs that are considered "verbose" (high frequency, can be toggled separately).
void runtime_log_verbose(const char *fmt, ...);

// Runtime logger backed by an in-memory ring buffer rendered on-screen.
void runtime_log_init(void);
void runtime_log(const char *fmt, ...);
void runtime_log_draw(int x, int y);
void runtime_log_clear(void);
void runtime_log_close(void);
// Enable or disable on-screen runtime logging
void runtime_log_enable(bool enabled);
void runtime_log_set_mode(runtime_log_mode_t mode);
runtime_log_mode_t runtime_log_get_mode(void);
void runtime_log_set_sprite_page(int page);
int runtime_log_get_sprite_page(void);
int runtime_log_get_sprite_page_count(void);

#endif
