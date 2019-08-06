#include "character_encoding.h"
#include <assert.h>

#ifdef _WIN32
#include <windows.h>

namespace Potato::Encoding
{

	size_t ansis_to_utf16s_require_space(const char* input, size_t input_length, uint32_t code_page) noexcept
	{
		return MultiByteToWideChar(code_page, 0, input, static_cast<int>(input_length), nullptr, 0);
	}

	size_t ansis_to_utf16s(const char* input, size_t input_length, char16_t* output, size_t output_length, uint32_t code_page) noexcept
	{
		if (output_length != 0)
		{
			return MultiByteToWideChar(code_page, 0, input, static_cast<int>(input_length), reinterpret_cast<wchar_t*>(output), static_cast<int>(output_length));
		}
		return 0;
	}

	size_t utf16s_to_ansis_require_space(const char16_t* input, size_t input_length, uint32_t code_page) noexcept
	{
		BOOL unchangleble = true;
		return WideCharToMultiByte(code_page, 0, reinterpret_cast<const wchar_t*>(input), static_cast<int>(input_length), nullptr, 0, "?", &unchangleble);
	}

	size_t utf16s_to_ansis(const char16_t* input, size_t input_length, char* output, size_t output_length, uint32_t code_page) noexcept
	{
		if (output_length != 0)
		{
			BOOL unchangleble = true;
			return WideCharToMultiByte(code_page, 0, reinterpret_cast<const wchar_t*>(input), static_cast<int>(input_length), output, static_cast<int>(output_length), "?", &unchangleble);
		}
		return 0;
	}
}


#endif

namespace Potato::Encoding
{
	size_t ansi_require_space(char da) noexcept
	{
		return static_cast<uint8_t>(da) > 127 ? 2 : 1;
	}

	size_t utf8_require_space(char da) noexcept
	{
		if ((da & 0xFE) == 0xFC)
			return 6;
		else if ((da & 0xFC) == 0xF8)
			return 5;
		else if ((da & 0xF8) == 0xF0)
			return 4;
		else if ((da & 0xF0) == 0xE0)
			return 3;
		else if ((da & 0xE0) == 0xC0)
			return 2;
		else if ((da & 0x80) == 0)
			return 1;
		else
			return 0;
	}

	bool utf8_check_string(const char* input, size_t avalible, size_t require_space) noexcept
	{
		if (require_space != 0 && avalible >= require_space)
		{
			for (size_t i = 1; i < require_space; ++i)
			{
				if ((input[i] & 0xC0) != 0x80)
					return false;
			}
			return true;
		}
		return false;
	}

	std::optional<size_t> utf8_check_string(const char* input, size_t avalible) noexcept
	{
		if (avalible > 0)
		{
			size_t re_space = utf8_require_space(input[0]);
			if (utf8_check_string(input, avalible, re_space))
				return { re_space };
			return {};
		}
		return { 0 };
	}

	size_t utf16_require_space(char16_t da)
	{
		if (da >= 0xD800 && da <= 0xDBFF)
			return 2;
		return 1;
	}

	size_t utf32_to_utf8_require_space(char32_t input)
	{
		if ((input & 0xFFFFFF80) == 0)
			return 1;
		else if ((input & 0xFFFF'F800) == 0)
			return 2;
		else if ((input & 0xFFFF'0000) == 0)
			return 3;
		else if ((input & 0xFFE0'0000) == 0)
			return 4;
		else if ((input & 0xFC00'0000) == 0)
			return 5;
		else if ((input & 0x8000'0000) == 0)
			return 6;
		else
			return 0;
	}

	size_t utf32_to_utf16_require_space(char32_t input)
	{
		if (input >= 0x10000 && input <= 0x10FFFF)
			return 2;
		else
			return 1;
	}

	size_t utf32_to_utf8(char32_t input, char* buffer, size_t output_size)
	{
		size_t require_count = utf32_to_utf8_require_space(input);
		if (output_size >= require_count)
		{
			switch (require_count)
			{
			case 1:
				*buffer = static_cast<char>(input & 0x0000007F);
				break;
			case 2:
				*buffer = 0xC0 | static_cast<char>((input & 0x07C0) >> 6);
				*(buffer + 1) = 0x80 | static_cast<char>((input & 0x3F));
				break;
			case 3:
				*buffer = 0xE0 | static_cast<char>((input & 0xF000) >> 12);
				*(buffer + 1) = 0x80 | static_cast<char>((input & 0xFC0) >> 6);
				*(buffer + 2) = 0x80 | static_cast<char>((input & 0x3F));
				break;
			case 4:
				*buffer = 0x1E | static_cast<char>((input & 0x1C0000) >> 18);
				*(buffer + 1) = 0x80 | static_cast<char>((input & 0x3F000) >> 12);
				*(buffer + 2) = 0x80 | static_cast<char>((input & 0xFC0) >> 6);
				*(buffer + 3) = 0x80 | static_cast<char>((input & 0x3F));
				break;
			case 5:
				*buffer = 0xF8 | static_cast<char>((input & 0x300'0000) >> 24);
				*(buffer + 1) = 0x80 | static_cast<char>((input & 0xFC'0000) >> 18);
				*(buffer + 2) = 0x80 | static_cast<char>((input & 0x3'F000) >> 12);
				*(buffer + 3) = 0x80 | static_cast<char>((input & 0xFC0) >> 6);
				*(buffer + 4) = 0x80 | static_cast<char>((input & 0x3F));
				break;
			case 6:
				*buffer = 0xFC | static_cast<char>((input & 0x4000'0000) >> 30);
				*(buffer + 1) = 0x80 | static_cast<char>((input & 0x3F00'0000) >> 24);
				*(buffer + 2) = 0x80 | static_cast<char>((input & 0xFC'0000) >> 18);
				*(buffer + 3) = 0x80 | static_cast<char>((input & 0x3'F000) >> 12);
				*(buffer + 4) = 0x80 | static_cast<char>((input & 0xFC0) >> 6);
				*(buffer + 5) = 0x80 | static_cast<char>((input & 0x3F));
				break;
			}
			return require_count;
		}
		return 0;
	}

	size_t utf8_to_utf32(const char* input, size_t input_size, char32_t& output)
	{
		if (input_size != 0)
		{
			size_t require_space = utf8_require_space(input[0]);
			if (require_space <= input_size)
			{
				switch (require_space)
				{
				case 1: output = input[0]; break;
				case 2: output = ((input[0] & 0x1F) << 6) | (input[1] & 0x3F); break;
				case 3: output = ((input[0] & 0x0F) << 12) | ((input[1] & 0x3F) << 6) | (input[2] & 0x3F); break;
				case 4: output = ((input[0] & 0x07) << 18) | ((input[1] & 0x3F) << 12) | ((input[2] & 0x3F) << 6) | (input[3] & 0x3F); break;
				case 5: output = ((input[0] & 0x03) << 24) | ((input[1] & 0x3F) << 18) | ((input[2] & 0x3F) << 12) | ((input[3] & 0x3F) << 6) | (input[4] & 0x3F); break;
				case 6: output = ((input[0] & 0x01) << 30) | ((input[1] & 0x3F) << 24) | ((input[2] & 0x3F) << 18) | ((input[3] & 0x3F) << 12) | ((input[4] & 0x3F) << 6) | (input[5] & 0x3F); break;
				}
				return require_space;
			}
		}
		return 0;
	}

	size_t utf16_to_uft32(const char16_t* input, size_t input_size, char32_t& output)
	{
		if (input_size != 0)
		{
			size_t require_space = utf16_require_space(input[0]);
			if (input_size >= require_space)
			{
				switch (require_space)
				{
				case 1: output = *input; break;
				case 2: output = (((input[0] & 0x3FF) << 10) | (input[1] & 0x3FF)) + 0x10000; break;
				}
				return require_space;
			}

		}
		return 0;
	}

	size_t utf32_to_utf16(char32_t input, char16_t* buffer, size_t output_size)
	{
		size_t require_space = utf32_to_utf16_require_space(input);
		if (output_size >= require_space)
		{
			switch (require_space)
			{
			case 1: buffer[0] = static_cast<char16_t>(input); break;
			case 2:
				char32_t tem = input - 0x10000;
				buffer[0] = ((tem & 0xFFC00) >> 10) & 0xD800;
				buffer[0] = (tem & 0x3FF) & 0xDC00;
				break;
			}
			return require_space;
		}
		return 0;
	}

	std::tuple<size_t, size_t> utf8_to_utf16(const char* input, size_t input_size, char16_t* output, size_t output_size)
	{
		size_t _1 = 0;
		size_t _2 = 0;
		if (input_size != 0)
		{
			char32_t tem;
			_1 = utf8_to_utf32(input, input_size, tem);
			if (_1 != 0)
				_2 = utf32_to_utf16(tem, output, output_size);
		}
		return { _1, _2 };
	}

	std::tuple<size_t, size_t> utf16_to_utf8(const char16_t* input, size_t input_size, char* output, size_t output_size)
	{
		size_t _1 = 0;
		size_t _2 = 0;
		if (input != nullptr && input_size != 0)
		{
			char32_t tem;
			_1 = utf16_to_uft32(input, input_size, tem);
			if (_1 != 0)
				_2 = utf32_to_utf8(tem, output, output_size);
		}
		return { _1, _2 };
	}

	std::tuple<size_t, size_t> utf32s_to_utf8s(const char32_t* input, size_t input_size, char* output, size_t output_size) noexcept
	{
		size_t index = 0;
		size_t utf8_used = 0;
		if (input_size >= 0)
		{
			for (; index < input_size; ++index)
			{
				size_t used = utf32_to_utf8(input[index], output + utf8_used, output_size);
				if (used != 0)
				{
					output_size -= used;
					utf8_used += used;
				}
				else
					break;
			}
		}
		return { index, utf8_used };
	}

	std::tuple<size_t, size_t> utf8s_to_utf32s(const char* input, size_t input_size, char32_t* output, size_t output_size) noexcept
	{
		size_t input_index = 0;
		size_t utf32_index = 0;
		if (input_size != 0)
		{
			for (; input_index < input_size && output_size > 0;)
			{
				size_t used = utf8_to_utf32(input + input_index, input_size, output[utf32_index]);
				if (used != 0)
				{
					input_index += used;
					++utf32_index;
				}
				else
					break;
			}
		}
		return { input_index, utf32_index };
	}

	std::tuple<size_t, size_t> utf16s_to_utf8s(const char16_t* input, size_t input_size, char* output, size_t output_size) noexcept
	{
		size_t utf16_index = 0;
		size_t utf8_index = 0;
		if (input_size != 0)
		{
			for (; utf16_index < input_size;)
			{
				auto pair = utf16_to_utf8(input + utf16_index, input_size, output + utf8_index, output_size);
				if (std::get<0>(pair) != 0 && std::get<1>(pair) != 0)
				{
					utf16_index += std::get<0>(pair);
					utf8_index += std::get<1>(pair);
					output_size -= std::get<1>(pair);
				}
				else
					break;
			}
		}
		return { utf16_index, utf8_index };
	}

	std::tuple<size_t, size_t> utf8s_to_utf16s(const char* input, size_t input_size, char16_t* output, size_t output_size) noexcept
	{
		size_t utf16_index = 0;
		size_t utf8_index = 0;
		if (input_size != 0)
		{
			while (utf8_index < input_size)
			{
				auto pair = utf8_to_utf16(input + utf8_index, input_size, output + utf16_index, output_size);
				if (std::get<0>(pair) != 0 && std::get<1>(pair) != 0)
				{
					utf8_index += std::get<0>(pair);
					utf16_index += std::get<1>(pair);
					output_size -= std::get<1>(pair);
				}
				else
					break;
			}
		}
		return { utf8_index, utf16_index };
	}


	std::u32string utf8s_to_utf32s(const std::string& input)
	{
		std::u32string temporary(input.size(), U'\0');
		auto pair = utf8s_to_utf32s(input.data(), input.size(), temporary.data(), temporary.size());
		temporary.resize(std::get<1>(pair));
		return temporary;
	}

	std::string utf32s_to_utf8s(const std::u32string& input)
	{
		std::string temporary(input.size() * 6, u'\0');
		auto pair = utf32s_to_utf8s(input.data(), input.size(), temporary.data(), temporary.size());
		temporary.resize(std::get<1>(pair));
		return temporary;
	}

	std::string utf16s_to_utf8s(const std::u16string& input)
	{
		std::string temporary(input.size() * 4, u'\0');
		auto pair = utf16s_to_utf8s(input.data(), input.size(), temporary.data(), temporary.size());
		temporary.resize(std::get<1>(pair));
		return temporary;
	}

	std::u16string utf8s_to_utf16s(const std::string& input)
	{
		std::u16string temporary(input.size(), u'\0');
		auto pair = utf8s_to_utf16s(input.data(), input.size(), temporary.data(), temporary.size());
		temporary.resize(std::get<1>(pair));
		return temporary;
	}
}

namespace Potato::Encoding
{

	std::optional<size_t> encoding_wrapper<char>::require_space(char input) noexcept
	{
		if ((input & 0xFE) == 0xFC)
			return 6;
		else if ((input & 0xFC) == 0xF8)
			return 5;
		else if ((input & 0xF8) == 0xF0)
			return 4;
		else if ((input & 0xF0) == 0xE0)
			return 3;
		else if ((input & 0xE0) == 0xC0)
			return 2;
		else if ((input & 0x80) == 0)
			return 1;
		else
			return std::nullopt;
	}

	bool encoding_wrapper<char>::check(const char* input, size_t input_length) noexcept
	{
		assert(input_length >= 1);
		auto p = require_space(input[0]);
		assert(p && *p == input_length);
		for (size_t i = 1; i < input_length; ++i)
			if ((input[i] & 0B11000000) != 0B10000000)
				return false;
		return true;
	};

	size_t encoding_wrapper<char>::encoding(const char* input, size_t input_length, char* output, size_t output_length) noexcept
	{
		if (input_length >= output_length)
		{
			for (size_t i = 0; i < input_length; ++i)
				output[i] = input[i];
			return input_length;
		}
		else
			return 0;
	}

	size_t encoding_wrapper<char>::encoding(const char* input, size_t input_length, char32_t* output, size_t output_length) noexcept
	{
		if (output_length >= 1)
		{
			switch (input_length)
			{
			case 1: *output = input[0]; break;
			case 2: *output = ((input[0] & 0x1F) << 6) | (input[1] & 0x3F); break;
			case 3: *output = ((input[0] & 0x0F) << 12) | ((input[1] & 0x3F) << 6) | (input[2] & 0x3F); break;
			case 4: *output = ((input[0] & 0x07) << 18) | ((input[1] & 0x3F) << 12) | ((input[2] & 0x3F) << 6) | (input[3] & 0x3F); break;
			case 5: *output = ((input[0] & 0x03) << 24) | ((input[1] & 0x3F) << 18) | ((input[2] & 0x3F) << 12) | ((input[3] & 0x3F) << 6) | (input[4] & 0x3F); break;
			case 6: *output = ((input[0] & 0x01) << 30) | ((input[1] & 0x3F) << 24) | ((input[2] & 0x3F) << 18) | ((input[3] & 0x3F) << 12) | ((input[4] & 0x3F) << 6) | (input[5] & 0x3F); break;
			}
			return 1;
		}
		return 0;
	}

	size_t encoding_wrapper<char>::encoding(const char* input, size_t input_length, char16_t* output, size_t output_length) noexcept
	{
		char32_t tem;
		size_t re = encoding(input, input_length, &tem, 1);
		assert(re == 1);
		return encoding_wrapper<char32_t>::encoding(&tem, 1, output, output_length);
	}

	std::optional<size_t> encoding_wrapper<char16_t>::require_space(char16_t input) noexcept
	{
		if (input <= 0x00ffff)
			return 1;
		else if ((input & 0B11011000) == 0B11011000)
			return 2;
		else
			return std::nullopt;
	}

	bool encoding_wrapper<char16_t>::check(const char16_t* input, size_t input_length) noexcept
	{
		assert(input_length >= 1);
		auto p = require_space(input[0]);
		assert(p && *p == input_length);
		if (input_length == 2)
			return (input[2] & 0B11011100) == 0B11011100;
		return true;
	}

	size_t encoding_wrapper<char16_t>::encoding(const char16_t* input, size_t input_length, char* output, size_t output_length) noexcept
	{
		char32_t tem;
		size_t re = encoding_wrapper<char16_t>::encoding(input, input_length, &tem, 1);
		assert(re == input_length);
		return encoding_wrapper<char32_t>::encoding(&tem, 1, output, output_length);
	}

	size_t encoding_wrapper<char16_t>::encoding(const char16_t* input, size_t input_length, char32_t* output, size_t output_length) noexcept
	{
		if (output_length >= 1)
		{
			switch (input_length)
			{
			case 1: *output = *input; break;
			case 2: *output = (char32_t(input[0] & 0B0000001111111111) << 10) + char32_t(input[1] & 0B0000001111111111) + 0x10000; break;
			}
			return 1;
		}
		return 0;
	}

	size_t encoding_wrapper<char16_t>::encoding(const char16_t* input, size_t input_length, char16_t* output, size_t output_length) noexcept
	{
		if (input_length <= output_length)
		{
			for (size_t i = 0; i < input_length; ++i)
				output[i] = input[i];
			return input_length;
		}
		return 0;
	}

	std::optional<size_t> encoding_wrapper<char32_t>::require_space(char32_t input) noexcept
	{
		if (input <= 0x10ffff)
			return 1;
		return std::nullopt;
	}

	bool encoding_wrapper<char32_t>::check(const char32_t* input, size_t input_length) noexcept { return input_length == 1; }

	size_t encoding_wrapper<char32_t>::encoding(const char32_t* input, size_t input_length, char* output, size_t output_length) noexcept
	{
		if ((input[0] & 0xFFFFFF80) == 0)
		{
			if (output_length >= 1)
			{
				output[0] = static_cast<char>(input[0] & 0x0000007F);
				return 1;
			}
		}
		else if ((input[0] & 0xFFFF'F800) == 0)
		{
			if (output_length >= 2)
			{
				output[0] = 0xC0 | static_cast<char>((*input & 0x07C0) >> 6);
				output[1] = 0x80 | static_cast<char>((*input & 0x3F));
				return 2;
			}
		}
		else if ((input[0] & 0xFFFF'0000) == 0)
		{
			if (output_length >= 3)
			{
				output[0] = 0xE0 | static_cast<char>((*input & 0xF000) >> 12);
				output[1] = 0x80 | static_cast<char>((*input & 0xFC0) >> 6);
				output[2] = 0x80 | static_cast<char>((*input & 0x3F));
				return 3;
			}
		}
		else if ((input[0] & 0xFFE0'0000) == 0)
		{
			if (output_length >= 4)
			{
				output[0] = 0x1E | static_cast<char>((*input & 0x1C0000) >> 18);
				output[1] = 0x80 | static_cast<char>((*input & 0x3F000) >> 12);
				output[2] = 0x80 | static_cast<char>((*input & 0xFC0) >> 6);
				output[3] = 0x80 | static_cast<char>((*input & 0x3F));
				return 4;
			}
		}
		else if ((input[0] & 0xFC00'0000) == 0)
		{
			if (output_length >= 5)
			{
				output[0] = 0xF8 | static_cast<char>((*input & 0x300'0000) >> 24);
				output[1] = 0x80 | static_cast<char>((*input & 0xFC'0000) >> 18);
				output[2] = 0x80 | static_cast<char>((*input & 0x3'F000) >> 12);
				output[3] = 0x80 | static_cast<char>((*input & 0xFC0) >> 6);
				output[4] = 0x80 | static_cast<char>((*input & 0x3F));
				return 5;
			}

		}
		else if ((input[0] & 0x8000'0000) == 0)
		{
			if (output_length >= 6)
			{
				output[0] = 0xFC | static_cast<char>((*input & 0x4000'0000) >> 30);
				output[1] = 0x80 | static_cast<char>((*input & 0x3F00'0000) >> 24);
				output[2] = 0x80 | static_cast<char>((*input & 0xFC'0000) >> 18);
				output[3] = 0x80 | static_cast<char>((*input & 0x3'F000) >> 12);
				output[4] = 0x80 | static_cast<char>((*input & 0xFC0) >> 6);
				output[5] = 0x80 | static_cast<char>((*input & 0x3F));
				return 6;
			}
		}
		else
			assert(false);
		return 0;
	}

	size_t encoding_wrapper<char32_t>::encoding(const char32_t* input, size_t input_length, char32_t* output, size_t output_length) noexcept
	{
		assert(input_length == 1);
		if (output_length >= 1)
			output[0] = input[0];
		return 1;
	}

	size_t encoding_wrapper<char32_t>::encoding(const char32_t* input, size_t input_length, char16_t* output, size_t output_length) noexcept
	{
		if (input[0] >= 0x10000)
		{
			if (output_length >= 2)
			{
				char32_t tem = input[0] - 0x10000;
				output[0] = ((tem & 0xFFC00) >> 10) & 0xD800;
				output[1] = (tem & 0x3FF) & 0xDC00;
				return 2;
			}
		}
		else
		{
			if (output_length >= 1)
			{
				output[0] = static_cast<char16_t>(input[0]);
				return 1;
			}
		}
		return 0;
	}

	const unsigned char utf8_bom[] = { 0xEF, 0xBB, 0xBF };
	const unsigned char utf16_le_bom[] = { 0xFF, 0xFE };
	const unsigned char utf16_be_bom[] = { 0xFE, 0xFF };
	const unsigned char utf32_le_bom[] = { 0x00, 0x00, 0xFE, 0xFF };
	const unsigned char utf32_be_bom[] = { 0xFF, 0xFe, 0x00, 0x00 };

	std::tuple<BomType, size_t>translate_binary_to_bomtype(const std::byte* bom, size_t bom_length) noexcept
	{
		assert(bom_length >= 4);
		if (std::memcmp(bom, utf8_bom, 3) == 0)
			return { BomType::UTF8, 3 };
		else if (std::memcmp(bom, utf32_le_bom, 4) == 0)
			return { BomType::UTF32LE, 4 };
		else if (std::memcmp(bom, utf32_be_bom, 4) == 0)
			return { BomType::UTF32BE, 4 };
		else if (std::memcmp(bom, utf16_le_bom, 2) == 0)
			return { BomType::UTF16LE, 2 };
		else if (std::memcmp(bom, utf16_be_bom, 2) == 0)
			return { BomType::UTF16BE, 2 };
		else
			return { BomType::None, 0 };
	}

	std::tuple<const std::byte*, size_t> translate_bomtype_to_binary(BomType format) noexcept
	{
		switch (format)
		{
		case BomType::UTF8: return { reinterpret_cast<const std::byte*>(utf8_bom), 3 };
		case BomType::UTF16LE: return { reinterpret_cast<const std::byte*>(utf16_le_bom), 2 };
		case BomType::UTF16BE: return { reinterpret_cast<const std::byte*>(utf16_be_bom), 2 };
		case BomType::UTF32LE: return { reinterpret_cast<const std::byte*>(utf32_le_bom), 4 };
		case BomType::UTF32BE: return { reinterpret_cast<const std::byte*>(utf32_be_bom), 4 };
		default: return { nullptr, 0 };
		}
	}
}