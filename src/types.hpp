#pragma once

#include <utility>
#include <variant>

#include <boost/asio.hpp>

namespace ac {
	namespace net = boost::asio;

	constexpr const std::size_t DEFAULT_BUFFER_SIZE = 1024;

	namespace socks {

		using buffet_t = std::array<uint8_t, DEFAULT_BUFFER_SIZE>;

		using req_addr_t = std::variant<net::ip::address, std::string>;

		inline std::string to_string(const req_addr_t& addr) {
			if (addr.index() == 0) {
				return std::get<0>(addr).to_string();
			} else {
				return std::get<1>(addr);
			}
		}

		struct req_t {
			req_addr_t addr;
			uint16_t   port;
		};

		// info for connect to upstream socks5 server
		struct req_info_t {
			req_t req;
			std::string username;
			std::string password;
		};
	}
}