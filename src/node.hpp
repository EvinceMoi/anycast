#pragma once

#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>

#include "types.hpp"
#include "config.hpp"
#include "io_context_pool.hpp"


namespace ac 
{
	namespace net = boost::asio;

	class node 
	{
		using acceptor_t = net::ip::tcp::acceptor;
		using socket_t = net::ip::tcp::socket;

	private:
		io_context_pool& iop_;
		config_t conf_;
		std::vector<acceptor_t> acceptors_;

	public:
		node(io_context_pool& iop, config_t&& conf);
		~node();

	public:
		boost::asio::awaitable<void> start();
		void stop();

	private:
		bool init_acceptors();
		boost::asio::awaitable<bool> do_accept();
		boost::asio::awaitable<void> start_accept_loop(acceptor_t& a);
		boost::asio::awaitable<void> start_proxy(socket_t socket);

		template <typename AsyncStream1, typename AsyncStream2>
		boost::asio::awaitable<void> do_forward(AsyncStream1& from, AsyncStream2& to) {
			using namespace boost::asio;

			boost::system::error_code ec;
			socks::buffet_t buf;
			for (;;) {
				auto bytes = co_await from.async_read_some(buffer(buf), redirect_error(use_awaitable, ec));
				if (ec)
					break;
				co_await to.async_write_some(buffer(buf, bytes), redirect_error(use_awaitable, ec));
				if (ec)
					break;
			}
			if constexpr (std::is_same_v<socket_t, std::decay_t<decltype(from)>>)
				LOG_DBG << "do_forward c -> p done";
			else
				LOG_DBG << "do_forward p -> c done";
		}
	};
}