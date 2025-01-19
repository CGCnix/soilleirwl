#pragma once

#include <stdint.h>

#define XCURSOR_MAGIC "Xcur"

#define SWL_XCURSOR_HEADER_SIZE_OFF 4
#define SWL_XCURSOR_HEADER_NTOC_OFF 12
#define SWL_XCURSOR_TYPE_COMMENT 0xfffe0001
#define SWL_XCURSOR_TYPE_IMAGE 0xfffd0002

typedef struct {
	uint32_t type;
	uint32_t subtype;
	uint32_t position;
} swl_xcur_toc_t;

typedef struct {
	const char magic[4];
	uint32_t size;
	uint32_t version;
	uint32_t ntoc;
	swl_xcur_toc_t toc[];
} swl_xcur_header_t;

typedef struct {
	uint32_t size;
	uint32_t type;
	uint32_t subtype;
	uint32_t version;
} swl_xcur_chunk_t;

typedef struct {
	swl_xcur_chunk_t hdr;
	uint32_t length;
	char string[];
} swl_xcur_string_t;

typedef struct {
	swl_xcur_chunk_t hdr;
	uint32_t width;
	uint32_t height;
	uint32_t xhot;
	uint32_t yhot;
	uint32_t delay;
	uint32_t pixels[];
} swl_xcur_image_t;
