#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define PAGE_SIZE 4096

int write_physmem(uint64_t target, uint64_t width, uint64_t val)
{
    void *map_base, *virt_addr;
	uint64_t writeval = val; /* for compiler */
	unsigned page_size, mapped_size, offset_in_page;
	int fd;

    width = width * 8;

	fd = open("/dev/mem", (O_RDWR | O_SYNC));
	mapped_size = page_size = PAGE_SIZE;
	offset_in_page = (unsigned)target & (page_size - 1);
	if (offset_in_page + width > page_size) {
		/* This access spans pages.
		 * Must map two pages to make it possible: */
		mapped_size *= 2;
	}
	map_base = mmap(NULL,
			mapped_size,
			PROT_READ | PROT_WRITE,
			MAP_SHARED,
			fd,
			target & ~(off_t)(page_size - 1));
    
	if (map_base == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap\n");
        return EXIT_FAILURE;
    }

	virt_addr = (char*)map_base + offset_in_page;

    switch (width) {
    case 8:
        *(volatile uint8_t*)virt_addr = writeval;
        break;
    case 16:
        *(volatile uint16_t*)virt_addr = writeval;
        break;
    case 32:
        *(volatile uint32_t*)virt_addr = writeval;
        break;
    case 64:
        *(volatile uint64_t*)virt_addr = writeval;
        break;
    default:
		fprintf(stderr, "Bad physmem width: %u\n", width);
        return EXIT_FAILURE;
    }

    if (munmap(map_base, mapped_size) == -1) {
		fprintf(stderr, "Failed to munmap\n");
        return EXIT_FAILURE;
    }
    close(fd);

	return EXIT_SUCCESS;
}

int read_physmem(uint64_t target, uint64_t width, uint64_t *val)
{
    void *map_base, *virt_addr;
	unsigned page_size, mapped_size, offset_in_page;
	int fd;

    width = width * 8;

	fd = open("/dev/mem", (O_RDONLY | O_SYNC));
	mapped_size = page_size = PAGE_SIZE;
	offset_in_page = (unsigned)target & (page_size - 1);
	if (offset_in_page + width > page_size) {
		/* This access spans pages.
		 * Must map two pages to make it possible: */
		mapped_size *= 2;
	}
	map_base = mmap(NULL,
			mapped_size,
			PROT_READ,
			MAP_SHARED,
			fd,
			target & ~(off_t)(page_size - 1));
	if (map_base == MAP_FAILED) {
		fprintf(stderr, "Failed to mmap\n");
        return EXIT_FAILURE;
    }

	virt_addr = (char*)map_base + offset_in_page;

    switch (width) {
    case 8:
        *(uint8_t *)val = *(volatile uint8_t*)virt_addr;
        break;
    case 16:
        *(uint16_t *)val = *(volatile uint16_t*)virt_addr;
        break;
    case 32:
        *(uint32_t *)val = *(volatile uint32_t*)virt_addr;
        break;
    case 64:
        *(uint64_t *)val = *(volatile uint64_t*)virt_addr;
        break;
    default:
		fprintf(stderr, "Bad physmem width: %u\n", width);
        return EXIT_FAILURE;
    }

    if (munmap(map_base, mapped_size) == -1) {
		fprintf(stderr, "Failed to munmap\n");
        return EXIT_FAILURE;
    }
    close(fd);

	return EXIT_SUCCESS;

}

int main(void)
{
    write_physmem(0xB0030000, 4, 0x02020202);
	return 0;
}