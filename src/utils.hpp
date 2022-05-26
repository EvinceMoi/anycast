
#pragma once

#include <string>
#include <string_view>
#include <optional>

#include <boost/asio.hpp>

namespace ac::utils 
{
	template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
	// explicit deduction guide (not needed as of C++20)
	//template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;


	template <typename E>
	constexpr auto to_underlying(E e) noexcept
	{
		return static_cast<std::underlying_type_t<E>>(e);
	}
	
	// 
	void make_tcp_endpoint(std::string_view s, boost::asio::ip::tcp::endpoint& endp, boost::system::error_code& ec);
	std::string to_string(boost::asio::ip::tcp::endpoint& endp);
};
