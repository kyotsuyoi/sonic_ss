#ifndef SPRITE_SAFE_H
#define SPRITE_SAFE_H

#include <jo/jo.h>

// Safe wrapper for creating sprite animations: validates base id and
// available tiles before calling the engine to avoid fatal errors.
int sprite_safe_create_anim(int base_id, int count, int duration);

// Returns true if sprite animation creation has not yet failed.
// Once it fails, subsequent calls will be skipped to avoid spamming the
// engine with failed requests.
bool sprite_safe_can_create_anim(void);
void sprite_safe_reset(void);

#endif
