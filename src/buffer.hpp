#pragma once

#include <type_traits>

namespace ac::buffer
{
	namespace detail
	{
		// big endian read
		template <
			typename T, 
			typename FwIt
		> requires std::is_arithmetic_v<T>
		T read_impl(FwIt& it)
		{
			T ret = 0;
			for (int i = 0; i < sizeof(T); i++) {
				ret <<= CHAR_BIT;
				ret |= static_cast<std::uint8_t>(*it);
				++it;
			}
			return ret;
		}

		template <
			typename T,
			typename FwIt
		> requires std::is_arithmetic_v<T>
		void write_impl(FwIt& it, T value)
		{
			for (int i = sizeof(T) - 1; i >= 0; i--) {
				*it++ = static_cast<uint8_t>(0xff & (value >> (i * CHAR_BIT)));
			}
		}
	}

	template <typename T, typename FwIt>
	auto read(FwIt& it)
	{
		return detail::read_impl<T>(it);
	}

	template <typename T, typename FwIt>
	void write(FwIt& it, T value)
	{
		detail::write_impl(it, value);
	}

	template <typename FwIt>
	std::string read_string(FwIt& it, std::size_t count)
	{
		std::string ret;
		ret.reserve(count);

		for (auto i = 0; i < count; i++) {
			ret += static_cast<char>(detail::read_impl<uint8_t>(it));
		}

		return ret;
	}

	template <typename FwIt>
	void write_string(FwIt& it, std::string str) 
	{
		for (auto const& c : str) {
			detail::write_impl<uint8_t>(it, static_cast<uint8_t>(c));
		}
	}
}