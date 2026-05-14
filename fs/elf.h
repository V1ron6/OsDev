/*
 * fs/elf.h - ELF Executable Format Definitions
 *
 * Structure definitions for ELF (Executable and Linkable Format).
 * Current: Structure definitions only
 * Future: ELF parsing, symbol resolution, dynamic loading
 *
 * ELF is the standard executable format for Unix/Linux systems.
 * It supports:
 * - Object files (.o)
 * - Executables (.out, no extension on Unix)
 * - Shared libraries (.so)
 * - Core dumps
 *
 * File structure:
 * - ELF header: Magic, type, architecture, entry point, section/program headers
 * - Program headers: Memory layout, segments to load
 * - Section headers: Code, data, symbols, strings
 * - Sections: .text (code), .data (init data), .bss (uninit data), .symtab, .strtab, etc.
 */

#ifndef FS_ELF_H
#define FS_ELF_H

#include <stdint.h>
#include "types.h"

/* =========================================================================
 * ELF MAGIC AND CONSTANTS
 * ========================================================================= */

#define ELF_MAGIC       0x464C457F  /* 0x7F 'E' 'L' 'F' */
#define ELFCLASS32      1           /* 32-bit architecture */
#define ELFDATA2LSB     1           /* Little-endian */
#define ELFVERSION      1           /* Current version */
#define ELFOSABI_SYSV   0           /* UNIX System V ABI */

/* ELF file type */
#define ET_EXEC         2           /* Executable file */
#define ET_REL          1           /* Relocatable file */
#define ET_DYN          3           /* Shared object file */

/* Machine type */
#define EM_386          3           /* Intel 80386 */

/* Section flags */
#define SHF_WRITE       0x1         /* Writable section */
#define SHF_ALLOC       0x2         /* Occupies memory */
#define SHF_EXECINSTR   0x4         /* Executable */

/* Program flags */
#define PF_X            0x1         /* Executable */
#define PF_W            0x2         /* Writable */
#define PF_R            0x4         /* Readable */

/* Program header type */
#define PT_LOAD         1           /* Loadable segment */
#define PT_DYNAMIC      3           /* Dynamic linking info */
#define PT_INTERP       3           /* Interpreter */
#define PT_NOTE         4           /* Notes */
#define PT_SHLIB        5           /* Shared library */
#define PT_PHDR         6           /* Program header table */

/* =========================================================================
 * ELF HEADER (32-BIT)
 * ========================================================================= */

typedef struct {
    uint32_t e_magic;               /* Magic number (0x7F, 'E', 'L', 'F') */
    uint8_t  e_class;               /* 32 or 64-bit (1 = 32-bit) */
    uint8_t  e_data;                /* Endianness (1 = little-endian) */
    uint8_t  e_version;             /* ELF version */
    uint8_t  e_osabi;               /* OS/ABI */
    uint8_t  e_abiversion;          /* ABI version */
    uint8_t  e_pad[7];              /* Padding (unused) */
    uint16_t e_type;                /* File type (ET_EXEC, ET_REL, etc.) */
    uint16_t e_machine;             /* Machine type (EM_386, etc.) */
    uint32_t e_version2;            /* ELF version again */
    uint32_t e_entry;               /* Entry point virtual address */
    uint32_t e_phoff;               /* Program header offset (bytes) */
    uint32_t e_shoff;               /* Section header offset (bytes) */
    uint32_t e_flags;               /* Processor flags */
    uint16_t e_ehsize;              /* ELF header size (bytes) */
    uint16_t e_phentsize;           /* Program header entry size */
    uint16_t e_phnum;               /* Program header count */
    uint16_t e_shentsize;           /* Section header entry size */
    uint16_t e_shnum;               /* Section header count */
    uint16_t e_shstrndx;            /* Section header string table index */
} __attribute__((packed)) elf_header_t;

/* =========================================================================
 * PROGRAM HEADER (32-BIT)
 * ========================================================================= */

typedef struct {
    uint32_t p_type;                /* Segment type (PT_LOAD, PT_DYNAMIC, etc.) */
    uint32_t p_offset;              /* Offset in file (bytes) */
    uint32_t p_vaddr;               /* Virtual address in memory */
    uint32_t p_paddr;               /* Physical address (usually ignored) */
    uint32_t p_filesz;              /* Size in file (bytes) */
    uint32_t p_memsz;               /* Size in memory (bytes) */
    uint32_t p_flags;               /* Segment flags (PF_R, PF_W, PF_X) */
    uint32_t p_align;               /* Alignment boundary (bytes) */
} __attribute__((packed)) elf_program_header_t;

/* =========================================================================
 * SECTION HEADER (32-BIT)
 * ========================================================================= */

typedef struct {
    uint32_t sh_name;               /* Name (offset in string table) */
    uint32_t sh_type;               /* Section type */
    uint32_t sh_flags;              /* Section flags */
    uint32_t sh_addr;               /* Virtual address */
    uint32_t sh_offset;             /* Offset in file (bytes) */
    uint32_t sh_size;               /* Size (bytes) */
    uint32_t sh_link;               /* Related section index */
    uint32_t sh_info;               /* Extra information */
    uint32_t sh_addralign;          /* Alignment boundary */
    uint32_t sh_entsize;            /* Entry size (if table) */
} __attribute__((packed)) elf_section_header_t;

/* =========================================================================
 * SYMBOL TABLE ENTRY
 * ========================================================================= */

typedef struct {
    uint32_t st_name;               /* Symbol name (offset in string table) */
    uint32_t st_value;              /* Symbol value (address or size) */
    uint32_t st_size;               /* Symbol size (bytes) */
    uint8_t  st_info;               /* Type and binding info */
    uint8_t  st_other;              /* Visibility and other info */
    uint16_t st_shndx;              /* Section header index */
} __attribute__((packed)) elf_symbol_t;

/* =========================================================================
 * ELF PARSING FUNCTIONS
 * ========================================================================= */

/**
 * elf_validate_header() - Check if ELF header is valid
 *
 * Args:
 *   header - Pointer to ELF header
 *
 * Returns: true if header is valid 32-bit executable
 */
bool elf_validate_header(elf_header_t *header);

/**
 * elf_get_program_header() - Get program header by index
 *
 * Args:
 *   base  - Base address of ELF file in memory
 *   index - Program header index
 *
 * Returns: Pointer to program header, or NULL if invalid
 */
elf_program_header_t *elf_get_program_header(void *base, uint32_t index);

/**
 * elf_get_section_header() - Get section header by index
 *
 * Args:
 *   base  - Base address of ELF file in memory
 *   index - Section header index
 *
 * Returns: Pointer to section header, or NULL if invalid
 */
elf_section_header_t *elf_get_section_header(void *base, uint32_t index);

/**
 * elf_get_section_by_name() - Find section by name
 *
 * Args:
 *   base - Base address of ELF file in memory
 *   name - Section name (e.g., ".text", ".data")
 *
 * Returns: Pointer to section header, or NULL if not found
 */
elf_section_header_t *elf_get_section_by_name(void *base, const char *name);

#endif /* FS_ELF_H */
