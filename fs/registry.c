#include "fs/registry.h"
#include "bb_api.h"
#include "serial.h"
#include "fs/vfs.h"
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
static bool registry_persistence_enabled;

static void registry_persist(void) {
    char line[REGISTRY_KEY_LENGTH + REGISTRY_VALUE_LENGTH + 2];
    int32_t fd;

    if (!registry_persistence_enabled) {
        return;
    }
    fd = vfs_open("/etc/registry.db",
                  BB_O_WRONLY | BB_O_CREAT | BB_O_TRUNC);
    if (fd < 0) {
        return;
    }
    for (uint32_t i = 0; i < REGISTRY_MAX_ENTRIES; i++) {
        uint32_t path_length;
        uint32_t value_length;
        uint32_t line_length;

        if (!entries[i].used) {
            continue;
        }
        path_length = strlen(entries[i].path);
        value_length = strlen(entries[i].value);
        memcpy(line, entries[i].path, path_length);
        line[path_length] = '=';
        memcpy(line + path_length + 1, entries[i].value, value_length);
        line_length = path_length + value_length + 1;
        line[line_length++] = '\n';
        if (vfs_write(fd, line, line_length) != (int32_t)line_length) {
            break;
        }
    }
    vfs_close(fd);
}

static bool registry_load(void) {
    char buffer[4096];
    uint32_t used = 0;
    int32_t fd = vfs_open("/etc/registry.db", 0);

    if (fd < 0) {
        return false;
    }
    int32_t amount = vfs_read(fd, buffer, sizeof(buffer) - 1);
    vfs_close(fd);
    if (amount <= 0) {
        return false;
    }
    buffer[amount] = '\0';

    while (used < (uint32_t)amount) {
        uint32_t line_start = used;
        uint32_t equals = 0;
        uint32_t line_end;

        while (used < (uint32_t)amount && buffer[used] != '\n') {
            if (buffer[used] == '=' && equals == 0) {
                equals = used;
            }
            used++;
        }
        line_end = used;
        if (used < (uint32_t)amount) {
            used++;
        }
        if (equals > line_start && equals < line_end) {
            buffer[equals] = '\0';
            buffer[line_end] = '\0';
            registry_set(buffer + line_start, buffer + equals + 1);
        }
    }
    return true;
}

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
    registry_persistence_enabled = false;
    if (!registry_load()) {
        registry_set("HKLM/System/Console", "serial");
        registry_set("HKLM/System/Graphics", "disabled");
        registry_set("HKLM/System/ApiVersion", "0.3");
    }
    registry_persistence_enabled = true;
    registry_persist();
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
    registry_persist();
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
