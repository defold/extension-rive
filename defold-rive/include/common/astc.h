// Copyright 2020 The Defold Foundation
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_RIVE_ASTC_H
#define DM_RIVE_ASTC_H

#include <stdint.h>
#include <stddef.h>

namespace dmRive
{
	static const uint32_t ASTC_MAGIC = 0x5CA1AB13;

	struct ASTCHeader
	{
		uint32_t magic;
		uint8_t  block_width;
		uint8_t  block_height;
		uint8_t  block_depth;
		uint32_t width;
		uint32_t height;
		uint32_t depth;
	};

	// Check if data starts with ASTC magic bytes
	inline bool IsASTCData(const uint8_t* data, size_t data_size)
	{
		if (data_size < 4)
			return false;
		uint32_t magic = data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
		return magic == ASTC_MAGIC;
	}

	// Parse ASTC header from raw data. Returns false if invalid.
	inline bool ParseASTCHeader(const uint8_t* data, size_t data_size, ASTCHeader* out)
	{
		if (data_size < 16 || !IsASTCData(data, data_size))
			return false;

		out->magic        = ASTC_MAGIC;
		out->block_width  = data[4];
		out->block_height = data[5];
		out->block_depth  = data[6];

		// Dimensions are 24-bit little-endian
		out->width  = data[7]  | (data[8] << 8)  | (data[9] << 16);
		out->height = data[10] | (data[11] << 8) | (data[12] << 16);
		out->depth  = data[13] | (data[14] << 8) | (data[15] << 16);

		return true;
	}

	// Get pointer to compressed data (after 16-byte header)
	inline const uint8_t* GetASTCCompressedData(const uint8_t* data)
	{
		return data + 16;
	}

	// Get size of compressed data (total size minus 16-byte header)
	inline size_t GetASTCCompressedDataSize(size_t total_size)
	{
		return total_size > 16 ? total_size - 16 : 0;
	}
}

#endif // DM_RIVE_ASTC_H
