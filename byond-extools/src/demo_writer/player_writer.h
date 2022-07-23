#pragma once
#include "../core/core.h"

void init_clients();
void mark_client_login(unsigned short id);
void mark_client_logout(unsigned short id);
void mark_client_dirty(unsigned short id);
void flush_clients();
