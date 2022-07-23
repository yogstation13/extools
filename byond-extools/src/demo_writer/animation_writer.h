#pragma once
#include "../core/core.h"

void add_flick_to_queue(trvh icon, trvh atom);
void flush_flick_queue();
void add_animate_to_queue(trvh args, AnimatePtr oAnimate);
void flush_animation_queue();
