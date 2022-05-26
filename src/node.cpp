#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include "node.hpp"
#include "logging.hpp"
#include "socks.hpp"
#include "proxy.hpp"
#include "utils.hpp"

namespace ac {
	namespace net = boost::asio;

	node::node(io_context_pool& iop, config_t&& conf)
		: iop_(iop)
		, conf_(conf)
	{
	}

	node::~node() 
	{ 
		LOG_DBG << "~node()";
	}

	boost::asio::awaitable<void> node::start()
	{
		if (!init_acceptors()) {
			LOG_ERR << "do listen error";
			co_return;
		}
		if (!co_await do_accept()) {
			LOG_ERR << "do accept error";
			co_return;
		}
		co_return;
	}

	void node::stop() 
	{
		LOG_DBG << "stop()";
	}

	bool node::init_acceptors()
	{
		boost::system::error_code ec;

		if (conf_.bind_addrs.empty()) {
			return false;
		}

		// create acceptors
		for (auto const& bind_addr : conf_.bind_addrs) {
			net::ip::tcp::endpoint endp;
			utils::make_tcp_endpoint(bind_addr, endp, ec);
			if (ec) {
				LOG_ERR << "parse bind addr error: " << ec.message() << ", input: " << bind_addr;
				continue;
			}

			acceptor_t acceptor(iop_.get_io());
			acceptor.open(endp.protocol(), ec);
			if (ec) {
				LOG_ERR << "acceptor open error: " << ec.message();
				continue;
			}
			acceptor.set_option(net::socket_base::reuse_address(true), ec);
			if (ec) {
				LOG_ERR << "acceptor set_option error: " << ec.message();
				continue;
			}
			acceptor.bind(endp, ec);
			if (ec) {
				LOG_ERR << "acceptor bind endp error: " << ec.message();
				continue;
			}
			acceptor.listen(net::socket_base::max_listen_connections, ec);
			if (ec) {
				LOG_ERR << "acceptor listen error: " << ec.message();
				continue;
			}

			acceptors_.emplace_back(std::move(acceptor));
		}

		return !acceptors_.empty();
	}

	boost::asio::awaitable<bool> node::do_accept()
	{
		if (acceptors_.empty())
			co_return false;

		using boost::asio::experimental::promise;
		using boost::asio::experimental::use_promise;

		std::vector<promise<void(std::exception_ptr)>> promises;
		for (auto idx = 0; idx < iop_.pool_size(); idx++) {
			for (auto& a : acceptors_) {
				auto p = net::co_spawn(a.get_executor(), start_accept_loop(a), use_promise);
				promises.emplace_back(std::move(p));
			}
		}
		for (auto&& p : promises) {
			co_await p.async_wait(net::use_awaitable);
		}
		co_return true;
	}

	boost::asio::awaitable<void> node::start_accept_loop(acceptor_t& acceptor)
	{
		auto ex = co_await net::this_coro::executor;

		auto endp = acceptor.local_endpoint();
		LOG_INFO << "run accept on " << utils::to_string(endp);

		boost::system::error_code ec;
		for(;;) {
			socket_t socket(iop_.get_io());
			socket.set_option(net::ip::tcp::no_delay(true), ec);
			co_await acceptor.async_accept(socket, net::redirect_error(net::use_awaitable, ec));
			if (ec) {
				LOG_ERR << "async accept error:" << ec.message();

				if (ec == net::error::operation_aborted || ec == net::error::bad_descriptor) {
					break;
				}
				if (!acceptor.is_open()) {
					LOG_ERR << "acceptor not open";
					break;
				}

				continue;
			}

			net::co_spawn(ex, start_proxy(std::move(socket)), boost::asio::detached);
		}
		LOG_INFO << "accept loop stopped.";
		co_return;
	}

	boost::asio::awaitable<void> node::start_proxy(socket_t socket)
	{
		using namespace boost::asio::experimental::awaitable_operators;

		auto ex = co_await net::this_coro::executor;

		auto redp = socket.remote_endpoint();
		LOG_INFO << "start proxy for " << utils::to_string(redp);
		do {
			auto [ accept_ec, req ] = co_await socks::async_socks_accept(socket);
			if (accept_ec) {
				LOG_ERR << "proxy error: " << accept_ec.message();
				break;
			}

			auto p = std::make_shared<proxy<decltype(ex)>>(ex, conf_, req);
			bool ok = co_await p->connect_upstream();
			if (!ok) {
				LOG_ERR << "proxy error: upstream connect failed";
				break;
			}

			ok = co_await p->handshake();
			if (!ok) {
				LOG_ERR << "proxy handshake failed";
				break;
			}

			ok = co_await socks::async_socks_respond(socket, req, {});

			co_await (
				do_forward(socket, *p)
				&&
				do_forward(*p, socket)
			);

		} while (false);

		if (socket.is_open()) {
			socket.cancel();
			socket.close();
		}
		co_return;
	}

}