/*
 * createico
 * 
 * Copyright (c) 2021 Jon Olsson <jlo@wintermute.net>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb_image_write.h"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

namespace {

struct ICONDIR {
	std::uint16_t reserved = 0;
	std::uint16_t type = 1;
	std::uint16_t numImages = 0;
};
enum { ICONDIRSIZE = 6 };

struct ICONDIRENTRY {
	std::uint8_t width = 0;
	std::uint8_t height = 0;
	std::uint8_t numColors = 0;
	std::uint8_t reserved = 0;
	std::uint16_t colorPlanes = 0;
	std::uint16_t bitsPerPixel = 0;
	std::uint32_t size = 0;
	std::uint32_t offset = 0;
};
enum { ICONDIRENTRYSIZE = 16 };

struct Image {
	~Image() { std::free(data); }
	unsigned char * data = nullptr;
	int size = 0;
};

struct Images {
	explicit Images(unsigned numImages) : raw(numImages), png(numImages) {}
	std::vector<Image> raw;
	std::vector<Image> png;
};

} // !namespace

int
main(int argc, char ** argv)
{
	if (argc < 3 || argc > 4) {
		std::fprintf(stderr, "createico <ICO file> <256x256 image file> [16x16 image file]\n");
		return -1;
	}

	char const * const outputfile = argv[1];
	char const * const filename256 = argv[2];
	char const * const filename16 = argc == 4 ? argv[3] : nullptr;

	constexpr unsigned const resolutions[] = { 256, 72, 48, 32, 16 };
	constexpr unsigned const numImages = sizeof resolutions/sizeof(unsigned);
	constexpr unsigned const firstImage = 0;
	constexpr unsigned const lastImage = numImages - 1;

	Images images(numImages);

	int width256, height256, channels256;
	images.raw[firstImage].data = stbi_load(filename256, &width256, &height256, &channels256, 0);
	if (!images.raw[firstImage].data) {
		std::fprintf(stderr, "Failed to load/decode '%s'\n", filename256);
		return -1;
	}
	if (width256 != 256 || height256 != 256 || channels256 != 4) {
		std::fprintf(stderr, "'%s': not suitable image format: width: %u, height: %u, channels: %u\n", filename256, width256, height256, channels256);
		return -1;
	}

	int width16, height16, channels16;
	images.raw[lastImage].data = filename16 ? stbi_load(filename16, &width16, &height16, &channels16, 0) : nullptr;
	if (filename16 && !images.raw[lastImage].data) {
		std::fprintf(stderr, "Failed to load/decode '%s'\n", filename16);
		return -1;
	}
	if (filename16 && (width16 != 16 || height16 != 16 || channels16 != 4)) {
		std::fprintf(stderr, "'%s': not suitable image format: width: %u, height: %u, channels: %u\n", filename16, width16, height16, channels16);
		return -1;
	}

	for (unsigned i = 1; i < numImages; ++i) {
		if (!images.raw[i].data) {
			unsigned const res = resolutions[i];
			images.raw[i].data = (std::uint8_t*)std::malloc(res*res*4);
			if (!images.raw[i].data) {
				std::fprintf(stderr, "Failed to allocate memory for %ux%u image\n", res, res);
				return -1;
			}
		}
	}

	for (unsigned i = 1; i < numImages; ++i) {
		if (i != lastImage || !filename16) {
			unsigned const res = resolutions[i];
			if (!stbir_resize_uint8_srgb(images.raw[firstImage].data , 256, 256, 0, images.raw[i].data, res, res, 0, 4, 3, STBIR_FLAG_ALPHA_USES_COLORSPACE)) {
				std::fprintf(stderr, "Failed to downsample 256x256 to %ux%u\n", res, res);
				return -1;
			}
		}
	}

	for (unsigned i = 0; i < numImages; ++i) {
		unsigned const res = resolutions[i];
		images.png[i].data = stbi_write_png_to_mem(images.raw[i].data, 0, res, res, 4, &(images.png[i].size));
		if (!images.png[i].data) {
			std::fprintf(stderr, "Failed to PNG encode %ux%u\n", res, res);
			return -1;
		}
	}

	if (std::fopen(outputfile, "r")) {
		std::fprintf(stderr, "'%s': error: file already exists, refusing to overwrite it\n", outputfile);
		return -1;
	}

	FILE * of = std::fopen(outputfile, "wb");
	if (!of) {
		std::fprintf(stderr, "'%s': error: failed to open file for writing\n", outputfile);
		return -1;
	}

	ICONDIR icondir = { 0, 1, numImages };
	if (std::fwrite(&icondir, ICONDIRSIZE, 1, of) != 1) {
		std::fprintf(stderr, "'%s': error: failed to write ICONDIR header\n", outputfile);
		std::fclose(of);
		return -1;
	}

	int offset = ICONDIRSIZE + ICONDIRENTRYSIZE*numImages;
	for (unsigned i = 0; i < numImages; ++i) {
		unsigned const res = resolutions[i];
		constexpr std::uint8_t numColors = 0;	// non-palette = 0
		constexpr std::uint8_t reserved = 0;
		constexpr std::uint8_t colorPlanes = 1;
		constexpr std::uint8_t bitsPerPixel = 32;
		ICONDIRENTRY entry = {
			res == 256 ? 0u : (std::uint8_t)res, res == 256 ? 0u : (std::uint8_t)res,
			numColors, reserved, colorPlanes, bitsPerPixel,
			(std::uint32_t)images.png[i].size,
			(std::uint32_t)offset
		};
		if (std::fwrite(&entry, ICONDIRENTRYSIZE, 1, of) != 1) {
			std::fprintf(stderr, "'%s': error: failed to write ICONDIRENTRY %u\n", outputfile, i);
			std::fclose(of);
			return -1;
		}
		offset += images.png[i].size;
	}

	for (unsigned i = 0; i < numImages; ++i) {
		unsigned const res = resolutions[i];
		if (std::fwrite(images.png[i].data, images.png[i].size, 1, of) != 1) {
			std::fprintf(stderr, "'%s': error: failed to write %ux%u image\n", outputfile, res, res);
			std::fclose(of);
			return -1;
		}
	}

	std::fclose(of);

	std::fprintf(stdout, "'%s': success\n", outputfile);
}