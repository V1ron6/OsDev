#ifndef BYTEBANDIT_REGISTRY_H
#define BYTEBANDIT_REGISTRY_H

#include "types.h"

void registry_init(void);
int32_t registry_query(const char *path, char *buffer, uint32_t length);
int32_t registry_set(const char *path, const char *value);
void registry_list(void);

#endif
