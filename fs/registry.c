#include "fs/registry.h"
#include "bb_api.h"
#include "serial.h"
#include <string.h>

#define REGISTRY_MAX_ENTRIES 16
#define REGISTRY_KEY_LENGTH 64
#define REGISTRY_VALUE_LENGTH 128

typedef struct {
    char path[REGISTRY_KEY_LENGTH];
    char value[REGISTRY_VALUE_LENGTH];
    bool used;
} registry_entry_t;

static registry_entry_t entries[REGISTRY_MAX_ENTRIES];

static registry_entry_t *registry_find(const char *path) {
    for (uint32_t i = 0; i < REGISTRY_MAX_ENTRIES; i++) {
        if (entries[i].used && strcmp(entries[i].path, path) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}

void registry_init(void) {
    memset(entries, 0, sizeof(entries));
    registry_set("HKLM/System/Console", "serial");
    registry_set("HKLM/System/Graphics", "disabled");
    registry_set("HKLM/System/ApiVersion", "0.1");
}

int32_t registry_query(const char *path, char *buffer, uint32_t length) {
    registry_entry_t *entry = registry_find(path);
    uint32_t value_length;

    if (entry == NULL) {
        return BB_STATUS_NOT_FOUND;
    }
    if (buffer == NULL || length == 0) {
        return BB_STATUS_INVALID;
    }

    value_length = strlen(entry->value);
    if (value_length + 1 > length) {
        return BB_STATUS_AGAIN;
    }
    memcpy(buffer, entry->value, value_length + 1);
    return (int32_t)value_length;
}

int32_t registry_set(const char *path, const char *value) {
    registry_entry_t *entry = registry_find(path);

    if (path == NULL || value == NULL || strlen(path) >= REGISTRY_KEY_LENGTH ||
        strlen(value) >= REGISTRY_VALUE_LENGTH) {
        return BB_STATUS_INVALID;
    }
    if (entry == NULL) {
        for (uint32_t i = 0; i < REGISTRY_MAX_ENTRIES; i++) {
            if (!entries[i].used) {
                entry = &entries[i];
                entry->used = true;
                strncpy(entry->path, path, REGISTRY_KEY_LENGTH - 1);
                entry->path[REGISTRY_KEY_LENGTH - 1] = '\0';
                break;
            }
        }
    }
    if (entry == NULL) {
        return BB_STATUS_AGAIN;
    }
    strncpy(entry->value, value, REGISTRY_VALUE_LENGTH - 1);
    entry->value[REGISTRY_VALUE_LENGTH - 1] = '\0';
    return BB_STATUS_OK;
}

void registry_list(void) {
    serial_puts("Registry values:\n");
    for (uint32_t i = 0; i < REGISTRY_MAX_ENTRIES; i++) {
        if (entries[i].used) {
            serial_printf("  %s = %s\n", entries[i].path, entries[i].value);
        }
    }
}
