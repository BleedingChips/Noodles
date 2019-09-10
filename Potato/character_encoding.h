#pragma once
#include <string>
#include <tuple>
#include <optional>
namespace Potato::Encoding
{
	size_t ansis_to_utf16s_require_space(const char* input, size_t input_length, uint32_t code_page = 0) noexcept;
	size_t ansis_to_utf16s(const char* input, size_t input_length, char16_t* output, size_t output_length, uint32_t code_page = 0) noexcept;

	size_t utf16s_to_ansis_require_space(const char16_t* input, size_t input_length, uint32_t code_page = 0) noexcept;
	size_t utf16s_to_ansis(const char16_t* input, size_t input_length, char* output, size_t output_length, uint32_t code_page = 0) noexcept;

	size_t utf8_require_space(char da) noexcept;
	bool utf8_check_string(const char* input, size_t avalible, size_t require_space) noexcept;
	std::optional<size_t> utf8_check_string(const char* input, size_t avalible) noexcept;
	size_t utf16_require_space(char16_t da);

	size_t utf32_to_utf8_require_space(char32_t input);
	size_t utf32_to_utf16_require_space(char32_t input);

	size_t utf32_to_utf8(char32_t input, char* output_buffer, size_t output_size);
	size_t utf8_to_utf32(const char* input, size_t input_size, char32_t& output);

	size_t utf16_to_uft32(const char16_t* input, size_t input_size, char32_t& output);
	size_t utf32_to_utf16(char32_t input, char16_t* output, size_t output_size);

	std::tuple<size_t, size_t> utf8_to_utf16(const char* input, size_t input_size, char16_t* output, size_t output_size);
	std::tuple<size_t, size_t> utf16_to_utf8(const char16_t* input, size_t input_size, char* output, size_t avalible_buffer);

	std::tuple<size_t, size_t> utf32s_to_utf8s(const char32_t* input, size_t input_size, char* output, size_t output_size) noexcept;
	std::tuple<size_t, size_t> utf8s_to_utf32s(const char* input, size_t input_size, char32_t* output, size_t output_size) noexcept;
	std::tuple<size_t, size_t> utf16s_to_utf8s(const char16_t* input, size_t input_size, char* output, size_t output_size) noexcept;
	std::tuple<size_t, size_t> utf8s_to_utf16s(const char* input, size_t input_size, char16_t* output, size_t output_size) noexcept;

	std::string utf16s_to_utf8s(const std::u16string& utf16);
	std::u16string utf8s_to_utf16s(const std::string& utf16);
	std::u32string utf8s_to_utf32s(const std::string& input);
	std::string utf32s_to_utf8s(const std::u32string& input);
}

namespace Potato::Encoding
{
	template<typename type> struct encoding_wrapper;

	template<> struct encoding_wrapper<char> {
		static constexpr size_t max_space = 6;
		static std::optional<size_t> require_space(char input) noexcept;
		static bool check(const char* input, size_t input_length) noexcept;
		static size_t encoding(const char* input, size_t input_length, char* output, size_t output_length) noexcept;
		static size_t encoding(const char* input, size_t input_length, char32_t* output, size_t output_length) noexcept;
		static size_t encoding(const char* input, size_t input_length, char16_t* output, size_t output_length) noexcept;
		static size_t encoding(const char* input, size_t input_length, wchar_t* output, size_t output_length) noexcept
		{
			if constexpr (sizeof(wchar_t) == sizeof(char16_t))
				return encoding_wrapper::encoding(input, input_length, reinterpret_cast<char16_t*>(output), output_length);
			else if constexpr (sizeof(wchar_t) == sizeof(char32_t))
				return encoding_wrapper::encoding(input, input_length, reinterpret_cast<char32_t*>(output), output_length);
			else
				static_assert(sizeof(wchar_t) == sizeof(char16_t) || sizeof(wchar_t) == sizeof(char32_t));
		}
	};

	template<> struct encoding_wrapper<char16_t> {
		static constexpr size_t max_space = 2;
		static std::optional<size_t> require_space(char16_t input) noexcept;
		static bool check(const char16_t* input, size_t input_length) noexcept;
		static size_t encoding(const char16_t* input, size_t input_length, char* output, size_t output_length) noexcept;
		static size_t encoding(const char16_t* input, size_t input_length, char32_t* output, size_t output_length) noexcept;
		static size_t encoding(const char16_t* input, size_t input_length, char16_t* output, size_t output_length) noexcept;
		static size_t encoding(const char16_t* input, size_t input_length, wchar_t* output, size_t output_length) noexcept
		{
			if constexpr (sizeof(wchar_t) == sizeof(char16_t))
				return encoding_wrapper::encoding(input, input_length, reinterpret_cast<char16_t*>(output), output_length);
			else if constexpr (sizeof(wchar_t) == sizeof(char32_t))
				return encoding_wrapper::encoding(input, input_length, reinterpret_cast<char32_t*>(output), output_length);
			else
				static_assert(sizeof(wchar_t) == sizeof(char16_t) || sizeof(wchar_t) == sizeof(char32_t));
		}
	};

	template<> struct encoding_wrapper<char32_t> {
		static constexpr size_t max_space = 1;
		static std::optional<size_t> require_space(char32_t input) noexcept;
		static bool check(const char32_t* input, size_t input_length) noexcept;
		static size_t encoding(const char32_t* input, size_t input_length, char* output, size_t output_length) noexcept;
		static size_t encoding(const char32_t* input, size_t input_length, char32_t* output, size_t output_length) noexcept;
		static size_t encoding(const char32_t* input, size_t input_length, char16_t* output, size_t output_length) noexcept;
		static size_t encoding(const char32_t* input, size_t input_length, wchar_t* output, size_t output_length) noexcept
		{
			if constexpr (sizeof(wchar_t) == sizeof(char16_t))
				return encoding_wrapper::encoding(input, input_length, reinterpret_cast<char16_t*>(output), output_length);
			else if constexpr (sizeof(wchar_t) == sizeof(char32_t))
				return encoding_wrapper::encoding(input, input_length, reinterpret_cast<char32_t*>(output), output_length);
			else
				static_assert(sizeof(wchar_t) == sizeof(char16_t) || sizeof(wchar_t) == sizeof(char32_t));
		}
	};
	
	// length of output buffer used, length of input buffer used, state
	template<typename input_t, typename output_t>
	std::tuple<size_t, size_t, bool> encoding_one(const input_t* input, size_t input_length, output_t* output, size_t output_length) noexcept
	{
		if (input_length >= 1)
		{
			auto space = encoding_wrapper<input_t>::require_space(input[0]);
			if (space && encoding_wrapper<input_t>::check(input, *space))
			{
				if (*space <= input_length)
					return { encoding_wrapper<input_t>::encoding(input, input_length, output, output_length), *space, true };
			}
			return { 0, *space, false };
		}
		return { 0, 0, true };
	}

	template<typename input_t, typename output_t>
	// output buffer used, charactor count, input buffer used, state
	std::tuple<size_t, size_t, size_t, bool> encoding_string(const input_t* input, size_t input_length, output_t* output, size_t output_length) noexcept
	{
		size_t total = 0;
		size_t input_used = 0;
		size_t output_used = 0;
		while (true)
		{
			auto [ou, iu, state] = encoding_one(input + input_used, input_length - input_used, output + output_used, output_length - output_used);
			if (state)
			{
				if (ou != 0 && iu != 0)
				{
					total += 1;
					input_used += iu;
					output_used += ou;
				}
				else
					return { output_used ,total, input_used, true };
			}
			else
				return { output_used , total ,input_used, false };
		}
	}

	// output string, input used, state
	template<typename char_type, typename traits = std::char_traits<char_type>, typename allocatpr = std::allocator<char_type>, typename input_t>
	std::tuple<std::basic_string<char_type, traits, allocatpr>, size_t, bool> encoding_string(const input_t* input, size_t input_length) noexcept
	{
		std::basic_string<char_type, traits, allocatpr> result;
		char_type tem_buffer[encoding_wrapper<input_t>::max_space];
		size_t current_index = 0;
		while (true)
		{
			auto [iu, ou, state] = encoding_one(input + current_index, input_length - current_index, tem_buffer, encoding_wrapper<input_t>::max_space);
			if (state)
			{
				if (iu != 0 && ou != 0)
				{
					current_index += iu;
					result.insert(result.end(), tem_buffer, tem_buffer + ou);
				}
				else
					return { std::move(result), current_index, true };
			}
			else
				return { std::move(result), current_index, false };
		}
	}

	enum class BomType
	{
		None,
		UTF8,
		UTF16LE,
		UTF16BE,
		UTF32LE,
		UTF32BE
	};

	static constexpr size_t bom_length = 4;

	// bom type, bom length
	std::tuple<BomType, size_t>translate_binary_to_bomtype(const std::byte* bom, size_t bom_length) noexcept;

	std::tuple<const std::byte*, size_t> translate_bomtype_to_binary(BomType format) noexcept;
}