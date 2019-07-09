#include "document.h"
#include "character_encoding.h"
#include <assert.h>
#include <array>

namespace
{
	using namespace Potato::Encoding;

	const unsigned char utf8_bom[] = { 0xEF, 0xBB, 0xBF };
	const unsigned char utf16_le_bom[] = { 0xFF, 0xFE };
	const unsigned char utf16_be_bom[] = { 0xFE, 0xFF };
	const unsigned char utf32_le_bom[] = { 0x00, 0x00, 0xFE, 0xFF };
	const unsigned char utf32_be_bom[] = { 0xFF, 0xFe, 0x00, 0x00 };

	enum class BomType
	{
		None,
		UTF8,
		UTF16LE,
		UTF16BE,
		UTF32LE,
		UTF32BE
	};


	BomType translate_binary_to_bomtype(const unsigned char* bom) noexcept
	{
		if (std::memcmp(bom, utf8_bom, 3) == 0)
			return BomType::UTF8;
		else if (std::memcmp(bom, utf32_le_bom, 4) == 0)
			return BomType::UTF32LE;
		else if (std::memcmp(bom, utf32_be_bom, 4) == 0)
			return BomType::UTF32BE;
		else if (std::memcmp(bom, utf16_le_bom, 2) == 0)
			return BomType::UTF16LE;
		else if (std::memcmp(bom, utf16_be_bom, 2) == 0)
			return BomType::UTF16BE;
		else
			return BomType::None;
	}

	std::tuple<const unsigned char*, size_t> translate_bomtype_to_binary(BomType format) noexcept
	{
		switch (format)
		{
		case BomType::UTF8: return { utf8_bom, 3 };
		case BomType::UTF16LE: return { utf16_le_bom, 2 };
		case BomType::UTF16BE: return { utf16_be_bom, 2 };
		case BomType::UTF32LE: return { utf32_le_bom, 4 };
		case BomType::UTF32BE: return { utf32_be_bom, 4 };
		default: return { nullptr, 0 };
		}
	}

	int64_t calculate_bom_space(BomType format) noexcept
	{
		switch (format)
		{
		case BomType::UTF8:
			return 3;
		case BomType::UTF16LE:
		case BomType::UTF16BE:
			return 2;
		case BomType::UTF32BE:
		case BomType::UTF32LE:
			return 4;
		default:
			return 0;
		}
	}
}

namespace Potato :: Doc
{

	namespace Implement
	{
		Storage platform_storage_implement() noexcept
		{
			uint16_t da = 0x0102;
			if (*reinterpret_cast<char*>(&da) == 0x02)
				return Storage::LittleEnding;
			return Storage::BigEnding;
		}

		Storage platform_storage() noexcept
		{
			static const Storage storage = platform_storage_implement();
			return storage;
		}
	}

	size_t loader_binary::loader_execute(std::ifstream& file, void* output, size_t space)
	{
		assert(file.is_open());
		file.read(reinterpret_cast<char*>(output), space);
		return static_cast<size_t>(file.gcount());
	}

	size_t loader_binary::loader_execute_byte_swapping(std::ifstream& file, void* output, size_t space)
	{
		assert(file.is_open());
		file.read(reinterpret_cast<char*>(output), space);
		size_t readed_size = static_cast<size_t>(file.gcount());
		for(size_t s = 0, e = readed_size-1; s < e; ++s, --e)
			std::swap(reinterpret_cast<char*>(output)[s], reinterpret_cast<char*>(output)[e]);
		return readed_size;
	}

	loader_binary::loader_binary(const std::filesystem::path& path, Storage storage_type)
		: m_file(path, std::ios::binary)
	{
		set_storage(storage_type);
	}

	size_t loader_binary::read_array(void* array_start, size_t element_space, size_t element_count)
	{
		size_t readed_size = 0;
		for (size_t index = 0; index < element_count; ++index)
		{
			auto size = read(reinterpret_cast<std::byte*>(array_start) + element_space * index, element_space);
			if(size != 0)
				readed_size += size;
			else
				break;
		}
		return readed_size;
	}

	std::streamsize loader_binary::last_size()
	{
		if (is_open())
		{
			auto current = m_file.tellg();
			m_file.seekg(0, std::ios::end);
			auto end = m_file.tellg();
			m_file.seekg(current);
			return end - current;
		}
		return 0;
	}

	std::streamsize loader_binary::total_size()
	{
		if (is_open())
		{
			auto current = m_file.tellg();
			m_file.seekg(0, std::ios::end);
			auto end = m_file.tellg();
			m_file.seekg(current);
			return end;
		}
		return 0;
	}

	std::tuple<size_t, std::streampos> load_utf8_to_utf8(loader_binary& lb, std::array<char, 6>& output)
	{
		assert(lb.is_open());
		auto pos = lb.cursor();
		size_t read = lb.read(output.data(), 1);
		if (read == sizeof(char))
		{
			size_t count = Encoding::utf8_require_space(output[0]);
			read = lb.read_array(output.data() + 1, count -  1);
			if (count == read + 1 && Encoding::utf8_check_string(output.data(), 6, count))
			{
				return {count, pos };
			}
			else {
				lb.cursor(pos);
				throw loader_unsupport_format{};
			}
		}
		else if(read == 0){
			return { 0, pos };
		}
		else {
			lb.cursor(pos);
			throw loader_unsupport_format{};
		}
	}

	std::tuple<size_t, std::streampos> load_utf16_to_utf16(loader_binary& lb, std::array<char16_t, 2>& output)
	{
		assert(lb.is_open());
		auto pos = lb.cursor();
		size_t read = lb.read(output.data(), 1);
		if (read == sizeof(char16_t))
		{
			size_t count = Encoding::utf16_require_space(output[0]);
			read = lb.read_array(output.data() + 1, count - 1);
			if (read == (count - 1) * sizeof(char16_t))
				return { count , pos };
			else {
				lb.cursor(pos);
				throw loader_unsupport_format{};
			}
		}
		else if (read == 0)
		{
			return { 0, pos };
		}
		else {
			lb.cursor(pos);
			throw loader_unsupport_format{};
		}
	}

	std::tuple<size_t, std::streampos> load_utf32_to_utf32(loader_binary& lb, char32_t& output)
	{
		assert(lb.is_open());
		auto pos = lb.cursor();
		size_t read = lb.read(&output, 1);
		if (read == sizeof(char32_t))
			return { 1, pos };
		else if (read == 0)
			return { 0, pos };
		else {
			lb.cursor(pos);
			throw loader_unsupport_format{};
		}
	}

	void writer_binary::writer_execute(std::ofstream& file, const void* input, size_t input_size)
	{
		assert(file.is_open());
		file.write(reinterpret_cast<const char*>(input), input_size);
	}

	void writer_binary::writer_execute_byte_swapping(std::ofstream& file, const void* input, size_t space)
	{
		assert(file.is_open());
		if (space > 0)
		{
			for (--space; space > 0; --space)
			{
				file.write(reinterpret_cast<const char*>(input) + space, 1);
			}
		}
	}

	writer_binary::writer_binary(const std::filesystem::path& path, Storage storage_type)
		: m_file(path, std::ios::binary)
	{
		set_storage(storage_type);
	}

	writer_binary& writer_binary::write_array(const void* array, size_t elemnt_space, size_t element_count)
	{
		for (size_t index = 0; index < element_count; ++index)
			write(reinterpret_cast<const std::byte*>(array) + elemnt_space * index, elemnt_space);
		return *this;
	}

	BomType format_to_bom(Format format)
	{
		switch (format)
		{
		case Format::UTF8_WITH_BOM:
			return BomType::UTF8;
			/*
		case Format::UTF16BE:
			return BomType::UTF16BE;
		case Format::UTF16LE:
			return BomType::UTF16LE;
		case Format::UTF32LE:
			return BomType::UTF32LE;
		case Format::UTF32BE:
			return BomType::UTF32BE;
			*/
		default:
			return BomType::None;
		}
	}

	Format bom_to_format(BomType bt)
	{
		switch (bt)
		{
		case BomType::None:
			return Format::UTF8;
		case BomType::UTF8:
			return Format::UTF8_WITH_BOM;
		default:
			return Format::UnSupport;
		}
	}

	const char* loader_unsupport_format::what() const noexcept
	{
		return "un support format";
	}

	namespace Implement
	{

		loader_base::loader_base(const std::filesystem::path& path)
			: loader_binary(path, Storage::LittleEnding)
		{
			if (is_open())
			{
				unsigned char bom[4];
				loader_binary::read_array(bom, 4);
				auto bom_type = translate_binary_to_bomtype(bom);
				m_format = bom_to_format(bom_type);
				loader_binary::cursor(calculate_bom_space(bom_type));
			}
		}

		void loader_base::reset_cursor()
		{
			assert(is_open());
			auto bom_type = format_to_bom(m_format);
			loader_binary::cursor(calculate_bom_space(bom_type));
		}

		loader_utf8::loader_utf8(const std::filesystem::path& path)
			: loader_base(path)
		{
			if(is_open())
				reset_format(loader_base::m_format);
		}

		void loader_utf8::reset_format(Format f) noexcept
		{
			m_format = f;
			switch (f)
			{
			case Format::UTF8:
			case Format::UTF8_WITH_BOM:
				execute_function = load_utf8_to_utf8;
				break;
			default:
				assert(false);
			}
		}

		size_t loader_utf8::read_one(char* output, size_t output_length)
		{
			std::array<char, 6> buffer;
			auto re = (*execute_function)(*this, buffer);
			if (std::get<0>(re) <= output_length)
			{
				std::memcpy(output, buffer.data(), std::get<0>(re) * sizeof(char));
				return std::get<0>(re);
			}
			else {
				cursor(std::get<1>(re));
				return 0;
			}
		}

		std::tuple<bool, bool> loader_line_state::handle(std::tuple<size_t, std::streampos, bool, bool> result, loader_binary& lb)
		{
			if (std::get<0>(result) != 0)
			{
				switch (m_state)
				{
				case 0:
					if (std::get<2>(result)) { m_state = 1; return { false, false }; }
					else if (std::get<3>(result)) { return {false, true}; }
					else {
						return { true, false };
					}
				case 1:
					if (!std::get<3>(result)) { lb.cursor(std::get<1>(result)); }
					m_state = 0;
					return { false, true };
				default:
					assert(false);
					return { false, true };
				}
			}
			else
				return {false, true};
		}

		std::tuple<size_t, bool> loader_utf8::read_line(char* output, size_t output_length)
		{
			return loader_read_line<char, 6>{}.read_line(*this, output, output_length);
		}

		std::tuple<size_t, std::streampos, bool, bool> loader_utf8::load_line_one(std::array<char, 6>& buffer)
		{
			auto re = execute_function(*this, buffer);
			return { std::get<0>(re), std::get<1>(re), buffer[0] == '\r', buffer[0] == '\n' };
		}

		loader_utf16::loader_utf16(const std::filesystem::path& path)
			: loader_base(path)
		{
			if (is_open())
				reset_format(loader_base::m_format);
		}

		void loader_utf16::reset_format(Format f) noexcept
		{
			m_format = f;
			switch (f)
			{
			case Format::UTF8:
			case Format::UTF8_WITH_BOM:
				execute_function = loader_utf8_to_utf16;
				break;
			default:
				assert(false);
			}
		}

		std::tuple<size_t, std::streampos> loader_utf16::loader_utf8_to_utf16(loader_binary& file, std::array<char16_t, 2>& buffer)
		{
			std::array<char, 6> tem_buffer;
			auto re = load_utf8_to_utf8(file, tem_buffer);
			if (std::get<0>(re) > 0)
			{
				return { std::get<1>(utf8_to_utf16(tem_buffer.data(), std::get<0>(re), buffer.data(), 2)), std::get<1>(re) };
			}
			return { 0, std::get<1>(re) };
		}

		size_t loader_utf16::read_one(char16_t* output, size_t output_length)
		{
			assert(is_open());
			std::array<char16_t, 2> buffet;
			auto re = (*execute_function)(*this, buffet);
			if (output_length >= std::get<0>(re))
			{
				std::memcpy(output, buffet.data(), std::get<0>(re) * sizeof(char16_t));
				return std::get<0>(re);
			}
			cursor(std::get<1>(re));
			return 0;
		}

		std::tuple<size_t, std::streampos, bool, bool> loader_utf16::load_line_one(std::array<char16_t, 2>& buffer)
		{
			auto re = (*execute_function)(*this, buffer);
			return { std::get<0>(re), std::get<1>(re), buffer[0] == u'\r', buffer[0] == u'\n' };
		}

		std::tuple<size_t, bool> loader_utf16::read_line(char16_t* output, size_t output_length)
		{
			return loader_read_line<char16_t, 2>{}.read_line(*this, output, output_length);
		}

		loader_utf32::loader_utf32(const std::filesystem::path& path)
			: loader_base(path)
		{
			if (is_open())
				reset_format(m_format);
		}

		void loader_utf32::reset_format(Format f) noexcept
		{
			m_format = f;
			switch (f)
			{
			case Format::UTF8:
			case Format::UTF8_WITH_BOM:
				execute_function = loader_utf8_to_utf32;
				break;
			default:
				assert(false);
			}
		}

		std::tuple<size_t, std::streampos> loader_utf32::loader_utf8_to_utf32(loader_binary& file, std::array<char32_t, 1>& buffer)
		{
			std::array<char, 6> tem_buffer;
			auto re = load_utf8_to_utf8(file, tem_buffer);
			if (std::get<0>(re) > 0)
			{
				return {utf8_to_utf32(tem_buffer.data(), static_cast<size_t>(std::get<1>(re)), buffer[0]), std::get<1>(re) };
			}
			return { 0, std::get<1>(re) };
		}

		std::tuple<size_t, std::streampos, bool, bool> loader_utf32::load_line_one(std::array<char32_t, 1>& buffer)
		{
			auto re = execute_function(*this, buffer);
			return { std::get<0>(re), std::get<1>(re), buffer[0] == U'\r', buffer[0] == U'\n' };
		}

		size_t loader_utf32::read_one(char32_t* output, size_t output_length)
		{
			std::array<char32_t, 1> buffer;
			auto re = (*execute_function)(*this, buffer);
			if (std::get<0>(re) <= output_length)
			{
				*output = buffer[0];
				return std::get<0>(re);
			}
			else {
				cursor(std::get<1>(re));
				return 0;
			}
		}

		std::tuple<size_t, bool> loader_utf32::read_line(char32_t* output, size_t output_length)
		{
			return loader_read_line<char32_t, 1>{}.read_line(*this, output, output_length);
		}
	}

	namespace Implement
	{
		writer_base::writer_base(const std::filesystem::path& path, Format format)
			: writer_binary(path, Storage::LittleEnding), m_format(format)
		{
			if (is_open())
			{
				auto bom = format_to_bom(m_format);
				auto result = translate_bomtype_to_binary(bom);
				writer_binary::write_array(std::get<0>(result), std::get<1>(result));
			}
		}

		writer_utf8::writer_utf8(const std::filesystem::path& path, Format format)
			: writer_base(path, format)
		{
			if (is_open())
			{
				switch (m_format)
				{
				case Format::UTF8:
				case Format::UTF8_WITH_BOM:
					execute_function = writer_utf8_to_utf8;
					break;
				default:
					assert(false);
					break;
				}
			}
		}

		void writer_utf8::writer_utf8_to_utf8(Doc::writer_binary& file, const char* input, size_t length)
		{
			file.write_array(input, length);
		}

		void writer_utf16::writer_utf16_to_utf8(Doc::writer_binary& file, const char16_t* input, size_t length)
		{
			assert(file.is_open());
			for (size_t index = 0; index < length;)
			{
				char buffer[6];
				auto result = utf16_to_utf8(input + index, length - index, buffer, 6);
				file.write_array(buffer, std::get<1>(result));
				index += std::get<0>(result);
			}
		}

		writer_utf16::writer_utf16(const std::filesystem::path& path, Format format)
			: writer_base(path, format)
		{
			if (is_open())
			{
				switch (m_format)
				{
				case Format::UTF8:
				case Format::UTF8_WITH_BOM:
					execute_function = writer_utf16_to_utf8;
					break;
				default:
					assert(false);
					break;
				}
			}
		}

		void writer_utf32::writer_utf32_to_utf8(Doc::writer_binary& file, const char32_t* input, size_t length)
		{
			assert(file.is_open());
			for (size_t index = 0; index < length; ++index)
			{
				char buffer[6];
				size_t require = utf32_to_utf8(input[index], buffer, 6);
				file.write_array(buffer, require);
			}
		}

		writer_utf32::writer_utf32(const std::filesystem::path& path, Format format)
			: writer_base(path, format)
		{
			if (is_open())
			{
				switch (m_format)
				{
				case Format::UTF8:
				case Format::UTF8_WITH_BOM:
					execute_function = writer_utf32_to_utf8;
					break;
				default:
					assert(false);
					break;
				}
			}
		}
	}
}