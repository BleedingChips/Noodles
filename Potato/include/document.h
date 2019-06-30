#pragma once

#include <filesystem>
#include <fstream>
#include <vector>
#include <map>
#include <assert.h>

namespace Potato ::Doc
{

	enum class Storage
	{
		LittleEnding,
		BigEnding
	};

	inline Storage operator!(Storage s) noexcept
	{
		switch (s)
		{
		case Storage::LittleEnding:
			return Storage::BigEnding;
		case Storage::BigEnding:
			return Storage::LittleEnding;
		default:
			assert(false);
			return Storage::LittleEnding;
		}
	}

	namespace Implement
	{
		Storage platform_storage() noexcept;
	}
	

	struct loader_binary
	{
		loader_binary() = default;
		loader_binary(const std::filesystem::path& path, Storage storage_type = Implement::platform_storage());
		loader_binary(loader_binary&&) = default;
		loader_binary& operator=(loader_binary&&) = default;

		bool is_open() const { return m_file.is_open(); }
		bool is_end_of_file() const { return m_file.eof(); }
		std::streamsize last_size();
		std::streamsize total_size();
		std::streampos cursor() { return m_file.tellg(); }
		void cursor(std::streampos cursor) { m_file.seekg(cursor); }
		void close() { m_file.close(); }

		size_t read(void* output, size_t output_space) { return loader_execute(m_file, output, output_space); }
		size_t read_array(void* array_start, size_t element_space, size_t element_count);

		template<typename T, typename = std::enable_if_t<std::is_standard_layout_v<T> && (!std::is_pointer_v<T>)>>
		size_t read(T& output) { return read(reinterpret_cast<void*>(&output), sizeof(T)); }

		template<typename T, typename = std::enable_if_t<std::is_standard_layout_v<T>>>
		size_t read_array(T* output, size_t count) { return read_array(reinterpret_cast<void*>(output), sizeof(T), count); }

		void set_storage(Storage storage_type) { execute = ((storage_type == Implement::platform_storage()) ? loader_execute : loader_execute_byte_swapping); }
		Storage storage() const {
			return (execute == loader_execute) ? Implement::platform_storage() : !Implement::platform_storage();
		}
	private:
		size_t(*execute)(std::ifstream&, void*, size_t) = nullptr;
		//bool m_byte_swapping = false;
		std::ifstream m_file;
		static size_t loader_execute(std::ifstream&, void* output, size_t output_size);
		static size_t loader_execute_byte_swapping(std::ifstream&, void* output, size_t output_size);
	};

	std::tuple<size_t, std::streampos> load_utf8_to_utf8(loader_binary& lb, std::array<char, 6>& output);
	std::tuple<size_t, std::streampos> load_utf16_to_utf16(loader_binary& lb, std::array<char16_t, 2>& output);
	std::tuple<size_t, std::streampos> load_utf32_to_utf32(loader_binary& lb, char32_t& output);

	struct writer_binary
	{
		writer_binary() = default;
		writer_binary(const std::filesystem::path& path, Storage storage_type = Implement::platform_storage());
		writer_binary(writer_binary&&) = default;
		writer_binary& operator=(writer_binary&&) = default;

		bool is_open() const noexcept { return m_file.is_open(); }
		bool is_end_of_file() const noexcept { return m_file.eof(); }

		writer_binary& write(const void* input, size_t input_space) { execute(m_file, input, input_space); return *this; }
		writer_binary& write_array(const void* array, size_t elemnt_space, size_t element_count);
		template<typename T, typename = std::enable_if_t<std::is_standard_layout_v<T> && (!std::is_pointer_v<T>)>>
		writer_binary& write(const T& input) { return write(reinterpret_cast<const void*>(&input), sizeof(T)); }
		template<typename T, typename = std::enable_if_t<std::is_standard_layout_v<T>>>
		writer_binary& write_array(const T* input, size_t element_count) { return write_array(reinterpret_cast<const void*>(input), sizeof(T), element_count); }
		void close() noexcept { m_file.close(); }

		void set_storage(Storage storage_type) noexcept { execute = ((storage_type == Implement::platform_storage()) ? writer_execute : writer_execute_byte_swapping); }
		Storage storage() const noexcept {
			return (execute == writer_execute) ? Implement::platform_storage() : !Implement::platform_storage();
		}
	private:
		void(*execute)(std::ofstream&, const void*, size_t) = nullptr;
		std::ofstream m_file;
		static void writer_execute(std::ofstream&, const void* input, size_t input_size);
		static void writer_execute_byte_swapping(std::ofstream&, const void* input, size_t input_size);
	};

	enum class Format : uint32_t
	{
		UTF8 = 0,
		UTF8_WITH_BOM,
		UnSupport,
	};

	struct loader_unsupport_format
	{
		const char* what() const noexcept;
	};

	namespace Implement
	{
		struct loader_base : protected loader_binary
		{
			loader_base() = default;
			loader_base(loader_base&& lb) = default;
			loader_base& operator=(loader_base&&) = default;
			loader_base(const std::filesystem::path& path);

			bool is_open() const { return loader_binary::is_open(); }
			bool is_end_of_file() const { return loader_binary::is_end_of_file(); }
			//Format format() const { return m_format; }
			void reset_cursor();
			void close() { loader_binary::close(); }
		protected:
			Format m_format;
		};

		struct loader_line_state
		{
			uint32_t m_state = 0;
			// is normal charater, done
			std::tuple<bool, bool> handle(std::tuple<size_t, std::streampos, bool, bool> result, loader_binary& lb);
		};

		template<typename Char, size_t index> struct loader_read_line
		{
			template<typename Loader, typename CharTraits = std::char_traits<Char>, typename allocator = std::allocator<Char>>
			auto operator()(Loader& loader, std::basic_string<Char, CharTraits, allocator> input = std::basic_string<Char, CharTraits, allocator>{})
			{
				loader_line_state state;
				while (true)
				{
					std::array<Char, index> buffer;
					auto re = loader.load_line_one(buffer);
					bool need_input, done;
					std::tie(need_input, done) = state.handle(re, loader);
					if (need_input)
						input.insert(input.end(), buffer.begin(), buffer.begin() + std::get<0>(re));
					if (done)
						break;
				}
				return std::move(input);
			}
			template<typename Loader>
			std::tuple<size_t, bool> read_line(Loader& loader, Char* output, size_t output_length)
			{
				loader_line_state state;
				size_t cur_index = 0;
				while (true)
				{
					std::array<Char, index> buffer;
					auto re = loader.load_line_one(buffer);
					bool need_input, done;
					std::tie(need_input, done) = state.handle(re, loader);
					if (need_input)
					{
						if (output_length >= std::get<0>(re) + cur_index)
						{
							std::memcpy(output + cur_index, buffer.data(), std::get<0>(re) * sizeof(Char));
							cur_index += std::get<0>(re);
						}
						else {
							loader.cursor(std::get<1>(re));
							return { cur_index, false };
						}
					}
					if (done)
						break;
				}
				return { cur_index, true };
			}
		};

		struct loader_utf8 : loader_base
		{
			loader_utf8() = default;
			loader_utf8(const std::filesystem::path& path);
			loader_utf8(loader_utf8&& lu) = default;
			loader_utf8& operator=(loader_utf8&&) = default;

			template<typename CharTraits = std::char_traits<char>, typename allocator = std::allocator<char>>
			auto read_line(std::basic_string<char, CharTraits, allocator> input = std::string{}) -> std::basic_string<char, CharTraits, allocator>
			{
				return loader_read_line<char, 6>{}(*this, std::move(input));
			}
			std::tuple<size_t, bool> read_line(char* output, size_t output_length);
			size_t read_one(char* output, size_t output_length);
		protected:
			void reset_format(Format f) noexcept;
			using CharT = char;
		protected:
			friend struct loader_utf16;
			friend struct loader_utf32;
			std::tuple<size_t, std::streampos, bool, bool> load_line_one(std::array<char, 6>& buffer);
			std::tuple<size_t, std::streampos>(*execute_function)(loader_binary& file, std::array<char, 6>& buffer) = nullptr;
			friend struct loader_read_line<char, 6>;
		};

		struct loader_utf16 : loader_base
		{
			loader_utf16() = default;
			loader_utf16(const std::filesystem::path& path);
			loader_utf16(loader_utf16&& lu) = default;
			loader_utf16& operator=(loader_utf16&&) = default;

			template<typename CharTraits = std::char_traits<char16_t>, typename allocator = std::allocator<char16_t>>
			auto read_line(std::basic_string<char16_t, CharTraits, allocator> input = std::u16string{}) -> std::basic_string<char16_t, CharTraits, allocator>
			{
				return loader_read_line<char16_t, 2>{}(*this, std::move(input));
			}

			std::tuple<size_t, bool> read_line(char16_t* output, size_t output_length);
			size_t read_one(char16_t* output, size_t output_length) ;
		protected:
			void reset_format(Format f) noexcept;
			using CharT = char16_t;
		protected:
			std::tuple<size_t, std::streampos, bool, bool> load_line_one(std::array<char16_t, 2>& buffer);
			std::tuple<size_t, std::streampos> (*execute_function)(loader_binary& file, std::array<char16_t, 2>& buffer) = nullptr;
			static std::tuple<size_t, std::streampos> loader_utf8_to_utf16(loader_binary&, std::array<char16_t, 2>& buffer);
			friend struct loader_read_line<char16_t, 2>;
		};

		struct loader_utf32 : loader_base
		{
			loader_utf32() = default;
			loader_utf32(loader_utf32&&) = default;
			loader_utf32(const std::filesystem::path& path);
			loader_utf32& operator=(loader_utf32&&) = default;

			template<typename CharTraits = std::char_traits<char32_t>, typename allocator = std::allocator<char32_t>>
			auto read_line(std::basic_string<char32_t, CharTraits, allocator> input = std::u32string{}) -> std::basic_string<char32_t, CharTraits, allocator>
			{
				return loader_read_line<char32_t, 1>{}(*this, std::move(input));
			}
			std::tuple<size_t, bool> read_line(char32_t* output, size_t output_length);
			size_t read_one(char32_t* output, size_t output_length = 1);
		protected:
			void reset_format(Format f) noexcept;
			using CharT = char16_t;
		private:
			std::tuple<size_t, std::streampos, bool, bool> load_line_one(std::array<char32_t, 1>& buffer);
			std::tuple<size_t, std::streampos> (*execute_function)(loader_binary& file, std::array<char32_t, 1>& buffer) = nullptr;
			static std::tuple<size_t, std::streampos> loader_utf8_to_utf32(loader_binary&, std::array<char32_t, 1>& buffer);
			friend struct loader_read_line<char32_t, 1>;
		};


		struct loader_wchar :
#ifdef _WIN32
			protected loader_utf16
		{
		private:
			using upper = loader_utf16;

#else
			protected loader_utf32
		{
		private:
			using upper = loader_utf32
#endif
		public:
			template<typename CharTraits = std::char_traits<wchar_t>, typename allocator = std::allocator<wchar_t>>
			auto read_line(std::basic_string<wchar_t, CharTraits, allocator> input = std::wstring{}) -> std::basic_string<wchar_t, CharTraits, allocator>
			{
				size_t current_size = input.size();
				size_t taregt_size = (current_size != 0 ? current_size : 10);
				input.resize(current_size + taregt_size);
				while (true)
				{
					auto re = read_line(input.data() + current_size, taregt_size);
					current_size += std::get<0>(re);
					if (!std::get<1>(re))
					{
						taregt_size = current_size;
						input.resize(current_size + taregt_size);
					}
					else {
						input.resize(current_size);
						return std::move(input);
					}
				}
			}
			std::tuple<size_t, bool> read_line(wchar_t* output, size_t output_length) { return upper::read_line(reinterpret_cast<upper::CharT*>(output), output_length); }
			size_t read_one(wchar_t* output, size_t output_length = 1) { return upper::read_one(reinterpret_cast<upper::CharT*>(output), output_length); }
			bool is_open() const { return upper::is_open(); }
			bool is_end_of_file() const { return upper::is_end_of_file(); }
			//Format format() const { return upper::format(); }
			void reset_cursor() { upper::reset_cursor(); }
			loader_wchar(const std::filesystem::path& path) : upper(path) {}
			void close() { upper::close(); }
			loader_wchar() = default;
			loader_wchar(loader_wchar&& lw) = default;
			loader_wchar& operator=(loader_wchar&&) = default;
		};

		template<typename T> struct loader_picker;
		template<> struct loader_picker<char> { using type = loader_utf8; };
		template<> struct loader_picker<wchar_t> { using type = loader_wchar; };
		template<> struct loader_picker<char16_t> { using type = loader_utf16; };
		template<> struct loader_picker<char32_t> { using type = loader_utf32; };
	}

	template<typename T> using loader = typename Implement::loader_picker<T>::type;

	namespace Implement
	{
		struct writer_base : private writer_binary
		{
			writer_base() = default;
			writer_base(writer_base&& eb) = default;
			writer_base& operator=(writer_base&& wb) = default;
			writer_base(const std::filesystem::path&, Format format = Format::UTF8);

			void close() { writer_binary::close(); }
			bool is_open() const { return writer_binary::is_open(); }
			Format format() const noexcept { return m_format; }
		protected:
			writer_binary& file() { return *this; }
			Format m_format;
		};

		struct writer_utf8 : writer_base
		{
			writer_utf8(const std::filesystem::path& path, Format format = Format::UTF8);
			writer_utf8() = default;
			writer_utf8(writer_utf8&& wu) = default;
			writer_utf8& operator=(writer_utf8&& wu) = default;
			writer_utf8& write(const char* input, size_t input_length) { assert(is_open()); (*execute_function)(file(), input, input_length); return *this; }
			writer_utf8& write(const char* input) { return write(input, std::char_traits<char>::length(input)); }
			template<typename T, typename P> writer_utf8& write(const std::basic_string<char, T, P>& p) { return write(p.data(), p.size()); }
		private:
			void(*execute_function)(Doc::writer_binary&, const char* input, size_t length) = nullptr;
			static void writer_utf8_to_utf8(Doc::writer_binary&, const char* input, size_t length);
		};

		struct writer_utf16 : writer_base
		{
			writer_utf16(const std::filesystem::path& path, Format format = Format::UTF8);
			writer_utf16() = default;
			writer_utf16(writer_utf16&& wu) = default;
			writer_utf16& operator=(writer_utf16&& wu) = default;
			writer_utf16& write(const char16_t* input, size_t input_length) { assert(is_open()); (*execute_function)(file(), input, input_length); return *this; }
			writer_utf16& write(const char16_t* input) { return write(input, std::char_traits<char16_t>::length(input)); }
			template<typename T, typename P> void write(const std::basic_string<char16_t, T, P>& p) { write(p.data(), p.size()); }
		private:
			void(*execute_function)(Doc::writer_binary&, const char16_t* input, size_t length) = nullptr;
			static void writer_utf16_to_ansi(Doc::writer_binary&, const char16_t* input, size_t length);
			static void writer_utf16_to_utf8(Doc::writer_binary&, const char16_t* input, size_t length);
			static void writer_utf16_to_utf16(Doc::writer_binary&, const char16_t* input, size_t length);
			static void writer_utf16_to_utf32(Doc::writer_binary&, const char16_t* input, size_t length);
		};

		struct writer_utf32 : writer_base
		{
			writer_utf32(const std::filesystem::path& path, Format format = Format::UTF8);
			writer_utf32() = default;
			writer_utf32(writer_utf32&& wu) = default;
			writer_utf32& operator=(writer_utf32&&) = default;
			writer_utf32& write(const char32_t* input, size_t input_length) { assert(is_open()); (*execute_function)(file(), input, input_length);  return *this; }
			writer_utf32& write(const char32_t* input) { return write(input, std::char_traits<char32_t>::length(input)); }
		protected:
			void(*execute_function)(Doc::writer_binary& o, const char32_t* input, size_t length) = nullptr;

			static void writer_utf32_to_ansi(Doc::writer_binary&, const char32_t* input, size_t length);
			static void writer_utf32_to_utf8(Doc::writer_binary&, const char32_t* input, size_t length);
			static void writer_utf32_to_utf16(Doc::writer_binary&, const char32_t* input, size_t length);
			static void writer_utf32_to_utf32(Doc::writer_binary&, const char32_t* input, size_t length);
		};

		struct writer_wchar :
#ifdef _WIN32
			protected writer_utf16
		{
		private:
			using upper = writer_utf16;
		public:
			writer_wchar& write(const wchar_t* input, size_t input_length) { writer_utf16::write(reinterpret_cast<const char16_t*>(input), input_length); return *this; }
#else
			protected writer_utf32
		{
		private:
			using upper = writer_utf32;
		public:
			void write(const wchar_t* input, size_t input_length) { writer_utf32::write(reinterpret_cast<const char32_t*>(input), input_length); }
#endif
			
			writer_wchar& write(const wchar_t* input) { return write(input, std::char_traits<wchar_t>::length(input)); }
			bool is_open() const { return upper::is_open(); }
			Format format() const noexcept { return upper::format(); }
			void close() { upper::close(); }
			writer_wchar(const std::filesystem::path& path, Format default_format = Format::UTF8) : upper(path, default_format) {}
			writer_wchar() = default;
			writer_wchar(writer_wchar&& lw) = default;
			writer_wchar& operator=(writer_wchar&&) = default;
		};

		template<typename T> struct writer_picker;
		template<> struct writer_picker<char> { using type = writer_utf8; };
		template<> struct writer_picker<wchar_t> { using type = writer_wchar; };
		template<> struct writer_picker<char16_t> { using type = writer_utf16; };
		template<> struct writer_picker<char32_t> { using type = writer_utf32; };
	}

	template<typename T> using writer = typename Implement::writer_picker<T>::type;

}

