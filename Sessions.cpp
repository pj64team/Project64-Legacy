/*
 * MiB64 - A Nintendo 64 emulator.
 *
 * (c) Copyright 2023 parasyte (jay@kodewerx.org)
 *
 * MiB64 Homepage: www.mib64.net
 *
 * Permission to use, copy, modify and distribute MiB64 in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * MiB64 is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for MiB64 or software derived from MiB64.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so if they want them.
 *
 */

#include <windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "cbor-lite/codec.h"

using namespace CborLite;

extern "C" {
	#include "Sessions.h"
}

// Constants

constexpr uint64_t SESSION_MEM_BOOKMARKS_VERSION = 1;

// Utility functions

std::string as_path(char *filename) {
	auto filepath = std::string(filename);

	// Support long file paths.
	// SEE: https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilea#parameters
	if (filepath.substr(0, 4) != std::string(R"(\\?\)")) {
		filepath.insert(0, R"(\\?\)");
	}

	return filepath;
}

bool read_cbor(std::vector<uint8_t> &buffer, char *filename) {
	auto filepath = as_path(filename);

	HANDLE file = CreateFile(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	DWORD len = GetFileSize(file, NULL);
	DWORD bytes_read = 0;

	buffer.clear();
	buffer.resize(len);
	BOOL read_success = (ReadFile(file, buffer.data(), len, &bytes_read, NULL) == TRUE);

	CloseHandle(file);

	return read_success && bytes_read == len;
}

bool write_cbor(char *filename, std::vector<uint8_t> &buffer) {
	auto filepath = as_path(filename);

	HANDLE file = CreateFile(filepath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		return false;
	}

	DWORD bytes_written;
	BOOL write_success = (WriteFile(file, buffer.data(), buffer.size(), &bytes_written, NULL) == TRUE);

	CloseHandle(file);

	return write_success && bytes_written == buffer.size();
}

// C++ implementations

template <typename InputIterator>
size_t decode_mem_bookmark(InputIterator &pos, InputIterator &end, struct MEM_BOOKMARK &bookmark, Flags flags = Flag::none) {
	uint64_t num_items = 0;
	auto len = decodeArraySize(pos, end, num_items, flags);
	if (num_items != 5) {
		throw Exception("expected 4 items");
	}

	auto name = std::string();
	len += decodeText(pos, end, name, flags);
	// Truncate name to fit in the buffer.
	strncpy(bookmark.name, name.c_str(), min(name.size(), sizeof(bookmark.name) - 1));
	bookmark.name[sizeof(bookmark.name) - 1] = 0;

	uint64_t width_required = 0;
	len += decodeUnsigned(pos, end, width_required, flags);
	// Truncate 64-bit to 32-bit.
	bookmark.width_required = (unsigned int)width_required;

	uint64_t selection_range = 0;
	len += decodeUnsigned(pos, end, selection_range, flags);
	bookmark.selection_range[0].UDW = selection_range;

	len += decodeUnsigned(pos, end, selection_range, flags);
	bookmark.selection_range[1].UDW = selection_range;

	bool is_virtual = true;
	len += decodeBool(pos, end, is_virtual, flags);
	// Convert bool to int.
	bookmark.is_virtual = is_virtual;

	return len;
}

template <typename Buffer>
size_t encode_mem_bookmark(Buffer &buffer, const struct MEM_BOOKMARK &bookmark) {
	auto len = encodeArraySize(buffer, 5u);

	auto name = std::string(bookmark.name);
	len += encodeText(buffer, name);
	len += encodeUnsigned(buffer, bookmark.width_required);
	len += encodeUnsigned(buffer, bookmark.selection_range[0].UDW);
	len += encodeUnsigned(buffer, bookmark.selection_range[1].UDW);

	bool is_virtual = bookmark.is_virtual;
	len += encodeBool(buffer, is_virtual);

	return len;
}

// C interfaces

BOOL Session_Load_MemBookmarks(unsigned int *num_bookmarks, struct MEM_BOOKMARK *bookmarks, unsigned int max_bookmarks, char *filename) {
	try {
		auto buffer = std::vector<uint8_t>();
		if (!read_cbor(buffer, filename)) {
			return FALSE;
		}

		auto pos = buffer.begin();
		auto end = buffer.end();

		// The version 1 schema looks like this:
		// [1, ["name", width_required, selection_range_start, selection_range_end, is_virtual], ...]
		// 
		// Example:
		// [1,
		//   ["Off : Debug [0x800015C0]", 138, 2147489216, 2147489226, true],
		//   ["Play Track Options [0x80001584]", 173, 2147489156, 2147489173, true]
		// ]
		uint64_t num_items = 0;
		decodeArraySize(pos, end, num_items);
		if (num_items < 1 || num_items - 1 > max_bookmarks) {
			return FALSE;
		}
		*num_bookmarks = (unsigned int)(num_items - 1);

		uint64_t version = 0;
		decodeUnsigned(pos, end, version);
		if (version != SESSION_MEM_BOOKMARKS_VERSION) {
			// NOTICE: Only version 1 is supported today.
			return FALSE;
		}

		for (unsigned int i = 0; i < *num_bookmarks; i++) {
			decode_mem_bookmark(pos, end, bookmarks[i]);
		}

		return TRUE;
	} catch (Exception) {
		return FALSE;
	}
}

BOOL Session_Save_MemBookmarks(char *filename, struct MEM_BOOKMARK *bookmarks, unsigned int num_bookmarks) {
	try {
		auto buffer = std::vector<uint8_t>();

		encodeArraySize(buffer, num_bookmarks + 1);
		encodeUnsigned(buffer, SESSION_MEM_BOOKMARKS_VERSION);

		for (unsigned int i = 0; i < num_bookmarks; i++) {
			encode_mem_bookmark(buffer, bookmarks[i]);
		}

		return write_cbor(filename, buffer);
	} catch (Exception) {
		return FALSE;
	}
}
