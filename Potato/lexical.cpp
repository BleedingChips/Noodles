#include "lexical.h"
#include "character_encoding.h"
#include <assert.h>
namespace Potato::Lexical
{

	uint64_t OctalToValue(const wchar_t* input, size_t input_size)
	{
		uint64_t value = 0;
		for (size_t index = 0; index < input_size; ++index)
		{
			auto& ref = input[index];
			assert(ref >= L'0' && ref <= L'7');
			value = value * 8 + ref - L'0';
		}
		return value;
	}

	uint64_t HexadecimalToValue(const wchar_t* input, size_t input_size)
	{
		uint64_t value = 0;
		for (size_t index = 0; index < input_size; ++index)
		{
			auto& ref = input[index];
			if (ref >= L'0' && ref <= L'9')
				value = value * 16 + ref - L'0';
			else if (ref >= L'a' && ref <= L'f')
				value = value * 16 + ref - L'a' + 10;
			else if (ref >= L'A' && ref <= L'F')
				value = value * 16 + ref - L'A' + 10;
			else
				assert(false);
		}
		return value;
	}

	const regex_token<EscapeSequence, wchar_t>& escape_sequence_cpp_wchar() noexcept
	{
		static regex_token<EscapeSequence, wchar_t> escape_sequence_cpp_wchar_implement{
		{ LR"(\\')", EscapeSequence::SingleQuote },
		{ LR"(\\")", EscapeSequence::DoubleQuote },
		{ LR"(\\\?)", EscapeSequence::QuestionMark },
		{ LR"(\\\\)", EscapeSequence::BackSlash },
		{ LR"(\\a)", EscapeSequence::AudibleBell },
		{ LR"(\\b)", EscapeSequence::BackSpace },
		{ LR"(\\f)", EscapeSequence::FormFeed },
		{ LR"(\\n)", EscapeSequence::LineFeed },
		{ LR"(\\r)", EscapeSequence::CarriageReturn },
		{ LR"(\\t)", EscapeSequence::HorizontalTab },
		{ LR"(\\v)", EscapeSequence::VerticalTab },
		{ LR"(\\[0-7]{1,3})", EscapeSequence::ArbitraryOctalValue },
		{ LR"(\\x([0-9]|[a-f]|[A-F]){1,2})", EscapeSequence::ArbitraryHexadecimalValue },
		{ LR"(\\u([0-9]|[a-f]|[A-F]){1,4})", EscapeSequence::UniversalCharacterName },
		{ LR"(\\U([0-9]|[a-f]|[A-F]){1,8})", EscapeSequence::UniversalCharacterName },
		{ LR"(\\[^\\]+)", EscapeSequence::NormalString },
		{ LR"([^\\]+)", EscapeSequence::NormalString }
		};
		return escape_sequence_cpp_wchar_implement;
	}



	size_t translate(EscapeSequence ES, const wchar_t* input, size_t input_size, wchar_t* output, size_t output_size) noexcept
	{
		if (output_size >= 1)
		{
			switch (ES)
			{
			case EscapeSequence::SingleQuote:
				output[0] = L'\'';
				return 1;
			case EscapeSequence::DoubleQuote:
				output[0] = L'\"';
				return 1;
			case EscapeSequence::QuestionMark:
				output[0] = L'\?';
				return 1;
			case EscapeSequence::BackSlash:
				output[0] = L'\\';
				return 1;
			case EscapeSequence::FormFeed:
				output[0] = L'\f';
				return 1;
			case EscapeSequence::LineFeed:
				output[0] = L'\n';
				return 1;
			case EscapeSequence::CarriageReturn:
				output[0] = L'\r';
				return 1;
			case EscapeSequence::HorizontalTab:
				output[0] = L'\t';
				return 1;
			case EscapeSequence::VerticalTab:
				output[0] = L'\t';
				return 1;
			case EscapeSequence::AudibleBell:
				output[0] = L'\a';
				return 1;
			case EscapeSequence::BackSpace:
				output[0] = L'\b';
				return 1;
			case EscapeSequence::ArbitraryOctalValue:
			{
				if (input_size >= 2)
				{
					output[0] = static_cast<wchar_t>(OctalToValue(input + 1, input_size - 1));
					return 1;
				}
				else
					assert(false);
			}
			case EscapeSequence::ArbitraryHexadecimalValue:
			{
				if (input_size >= 3)
				{
					output[0] = static_cast<wchar_t>(HexadecimalToValue(input + 2, input_size - 2));
					return 1;
				}
				else
					assert(false);
			}
			case EscapeSequence::UniversalCharacterName:
				if (input_size >= 3)
				{
					char32_t re = static_cast<char32_t>(HexadecimalToValue(input + 2, input_size - 2));
#ifdef _WIN32
					return Encoding::utf32_to_utf16(re, reinterpret_cast<char16_t*>(output), output_size);
#else
					output[0] = static_cast<wchar_t>(re);
					return 1;
#endif // _WIN32
				}
				else
					assert(false);
			case EscapeSequence::NormalString:
			{
				size_t size = (output_size > input_size ? input_size : output_size);
				std::memcpy(output, input, size * sizeof(wchar_t));
				return size;
			}
			default:
				assert(false);
			}
		}
		return 0;
	}

}