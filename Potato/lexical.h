#pragma once
#include <regex>
namespace Potato::Lexical
{

	template<typename Token, typename CharT, typename regex_traits = std::regex_traits<CharT>>
	struct regex_token
	{
		regex_token(std::initializer_list<std::tuple<const CharT*, Token>> list)
		{
			m_tokens.reserve(list.size());
			for (auto& ite : list)
				m_tokens.push_back({ std::basic_regex<CharT, regex_traits>{std::get<0>(ite), std::regex::optimize }, std::get<1>(ite) });
		}
	private:
		//Implement::
		std::vector<std::tuple< std::basic_regex<CharT, regex_traits>, Token>> m_tokens;
		friend struct regex_token_wrapper;
	};

#ifdef _WIN32
	template<typename Token>
	struct regex_token<Token, char16_t, std::regex_traits<char16_t>>
	{
		regex_token(std::initializer_list<std::tuple<const char16_t*, Token>> list)
		{
			m_tokens.reserve(list.size());
			for (auto& ite : list)
				m_tokens.push_back({ std::wregex{ reinterpret_cast<const wchar_t*>(std::get<0>(ite)), std::regex::optimize }, std::get<1>(ite) });
		}
	private:
		//Implement::
		std::vector<std::tuple<std::wregex, Token>> m_tokens;
		friend struct regex_token_wrapper;
	};
#endif

	enum class EscapeSequence
	{
		SingleQuote,
		BackSlash,
		CarriageReturn,
		HorizontalTab,
		DoubleQuote,
		QuestionMark,
		AudibleBell,
		BackSpace,
		FormFeed,
		LineFeed,
		VerticalTab,
		ArbitraryOctalValue,
		ArbitraryHexadecimalValue,
		UniversalCharacterName,
		NormalString,
	};

	const regex_token<EscapeSequence, wchar_t>& escape_sequence_cpp_wchar() noexcept;
	size_t translate(EscapeSequence, const wchar_t* input, size_t input_size, wchar_t* output, size_t output_size) noexcept;

	struct regex_token_wrapper
	{
		// Handler -> bool(Token, Ite, Ite) , UnTokenHandler -> bool(Ite, Ite);
		template<typename CharT, typename Ite, typename Token, typename Traits, typename Handler, typename UnTokenHandler>
		static bool generate(Ite begin, Ite end, const regex_token<Token, CharT, Traits>& regex, Handler&& H, UnTokenHandler&& UH)
		{
			if (begin != end)
			{
				while (true)
				{
					std::match_results<Ite> match;
					bool is_match = false;
					for (auto& ite : regex.m_tokens)
					{
						if (std::regex_search(begin, end, match, std::get<0>(ite), std::regex_constants::match_flag_type::match_continuous))
						{
							auto b = match[0].first;
							auto e = match[0].second;
							if (H(std::get<1>(ite), b, e) && e != end)
							{
								begin = e;
								is_match = true;
								break;
							}
							else
								return false;
						}
					}
					if (is_match || UH(begin, end));
					else
						break;

				}
				return true;
			}
			return false;
		}

	};

}