/*
 * fs/elf.c - ELF Loader Implementation Stubs
 *
 * Currently: Structure definitions and validation stubs only
 * Future: Full ELF loading and execution
 */

#include "fs/elf.h"
#include "types.h"
#include "mm/constants.h"
#include "mm/pmm.h"
#include "serial.h"
#include <string.h>

#define USER_ADDRESS_LIMIT 0xC0000000
#define ELF_LOAD_SCRATCH   0x00800000

static bool elf_range_valid(uint32_t offset, uint32_t length,
                            uint32_t image_size) {
    return offset <= image_size && length <= image_size - offset;
}

bool elf_validate_header(elf_header_t *header) {
    if (header == NULL) {
        return false;
    }
    
    /* Check magic number */
    if (header->e_magic != ELF_MAGIC) {
        return false;
    }
    
    /* Check architecture (32-bit) */
    if (header->e_class != ELFCLASS32) {
        return false;
    }
    
    /* Check endianness (little-endian) */
    if (header->e_data != ELFDATA2LSB) {
        return false;
    }
    
    /* Check executable type */
    if (header->e_type != ET_EXEC) {
        return false;
    }
    
    /* Check machine type (Intel 386) */
    if (header->e_machine != EM_386) {
        return false;
    }

    if (header->e_version != ELFVERSION || header->e_version2 != ELFVERSION ||
        header->e_ehsize != sizeof(elf_header_t) ||
        header->e_phentsize != sizeof(elf_program_header_t)) {
        return false;
    }
    
    return true;
}

elf_program_header_t *elf_get_program_header(void *base, uint32_t index) {
    if (base == NULL) {
        return NULL;
    }
    
    elf_header_t *header = (elf_header_t *)base;
    
    if (!elf_validate_header(header)) {
        return NULL;
    }
    
    if (index >= header->e_phnum) {
        return NULL;  /* Index out of range */
    }
    
    uint8_t *p = (uint8_t *)base + header->e_phoff;
    return (elf_program_header_t *)(p + (index * header->e_phentsize));
}

elf_section_header_t *elf_get_section_header(void *base, uint32_t index) {
    if (base == NULL) {
        return NULL;
    }
    
    elf_header_t *header = (elf_header_t *)base;
    
    if (!elf_validate_header(header)) {
        return NULL;
    }
    
    if (index >= header->e_shnum) {
        return NULL;  /* Index out of range */
    }
    
    uint8_t *p = (uint8_t *)base + header->e_shoff;
    return (elf_section_header_t *)(p + (index * header->e_shentsize));
}

elf_section_header_t *elf_get_section_by_name(void *base, const char *name) {
    if (base == NULL || name == NULL) {
        return NULL;
    }

    elf_header_t *header = (elf_header_t *)base;
    if (!elf_validate_header(header) ||
        header->e_shstrndx >= header->e_shnum) {
        return NULL;
    }

    elf_section_header_t *names = elf_get_section_header(base,
                                                          header->e_shstrndx);
    if (names == NULL) {
        return NULL;
    }

    const char *string_table = (const char *)base + names->sh_offset;
    for (uint32_t i = 0; i < header->e_shnum; i++) {
        elf_section_header_t *section = elf_get_section_header(base, i);
        if (section == NULL || section->sh_name >= names->sh_size) {
            continue;
        }

        if (strcmp(string_table + section->sh_name, name) == 0) {
            return section;
        }
    }

    return NULL;
}

bool elf_load_segments(void *base, uint32_t image_size,
                       page_directory_t *directory,
                       uint32_t *entry_point) {
    elf_header_t *header = (elf_header_t *)base;

    if (base == NULL || directory == NULL || entry_point == NULL ||
        image_size < sizeof(elf_header_t) || !elf_validate_header(header) ||
        !elf_range_valid(header->e_phoff,
                         (uint32_t)header->e_phnum * header->e_phentsize,
                         image_size) || header->e_entry >= USER_ADDRESS_LIMIT) {
        return false;
    }

    for (uint32_t i = 0; i < header->e_phnum; i++) {
        elf_program_header_t *program = elf_get_program_header(base, i);
        uint32_t segment_end;
        uint32_t page_start;

        if (program == NULL || program->p_type != PT_LOAD ||
            program->p_filesz > program->p_memsz ||
            !elf_range_valid(program->p_offset, program->p_filesz,
                             image_size) ||
            program->p_vaddr >= USER_ADDRESS_LIMIT ||
            program->p_memsz > USER_ADDRESS_LIMIT - program->p_vaddr) {
            return false;
        }

        segment_end = program->p_vaddr + program->p_memsz;
        page_start = program->p_vaddr & PAGE_MASK;
        while (page_start < segment_end) {
            uint32_t frame = pmm_alloc_frame();
            uint32_t page_end = page_start + PAGE_SIZE;
            uint32_t flags = PAGE_PRESENT | PAGE_USER;
            uint32_t copy_start = page_start;
            uint32_t copy_end = page_end;

            if (frame == 0) {
                return false;
            }
            if (program->p_flags & PF_W) {
                flags |= PAGE_WRITE;
            }
            if (copy_start < program->p_vaddr) {
                copy_start = program->p_vaddr;
            }
            if (copy_end > program->p_vaddr + program->p_filesz) {
                copy_end = program->p_vaddr + program->p_filesz;
            }

            if (!paging_map_page_in_directory(directory, page_start, frame,
                                               flags)) {
                return false;
            }
            if (!paging_map_page(ELF_LOAD_SCRATCH, frame, PAGE_KERNEL)) {
                return false;
            }
            memset((void *)ELF_LOAD_SCRATCH, 0, PAGE_SIZE);
            if (copy_start < copy_end) {
                memcpy((void *)(ELF_LOAD_SCRATCH + copy_start - page_start),
                       (const uint8_t *)base + program->p_offset +
                       (copy_start - program->p_vaddr),
                       copy_end - copy_start);
            }
            paging_unmap_page(ELF_LOAD_SCRATCH);
            page_start += PAGE_SIZE;
        }
    }

    *entry_point = header->e_entry;
    serial_printf("[ELF] Loaded entry point 0x%x\n", *entry_point);
    return true;
}
