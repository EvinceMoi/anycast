#include "utils.hpp"

#include <iostream>
#include <variant>

namespace ac::utils
{
	namespace net = boost::asio;
	using tcp = boost::asio::ip::tcp;

	void make_tcp_endpoint(std::string_view s, tcp::endpoint& endp, boost::system::error_code& ec) {
		do {
			bool is_v6 = s.find('[') == 0;

			std::string_view host, port;
			if (is_v6) {
				auto p = s.find(']');
				if (p == std::string_view::npos || (p + 2) >= s.length()) break;
				if (s.at(p + 1) != ':') break;

				host = s.substr(1, p - 1);
				port = s.substr(p + 2);
			} else {
				auto p = s.find(':');
				if (p == std::string_view::npos) break;

				host = s.substr(0, p);
				port = s.substr(p + 1);
			}

			endp.address(net::ip::make_address(host, ec));
			try {
				auto pn = std::stoul(std::string{port});
				if (pn > std::numeric_limits<std::uint16_t>::max()) break;
				endp.port(pn);
			} catch (std::exception& ex) {
				std::cout << "ex: " << ex.what() << std::endl;
				break;
			}

			return;
		} while (false);

		ec.assign(boost::system::errc::bad_address, boost::system::generic_category());
		return;
	}

	std::string to_string(tcp::endpoint& endp) {
		boost::system::error_code ingore_ec;
		
		auto address = endp.address();
		if (address.is_v6())
			return "[" + address.to_string(ingore_ec) + "]:" + std::to_string(endp.port());
		else
			return address.to_string(ingore_ec) + ":" + std::to_string(endp.port());
	}
}