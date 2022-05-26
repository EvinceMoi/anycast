#pragma once

#include <chrono>
//#include <memory>
#include <utility>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <boost/dynamic_bitset.hpp>

#include "types.hpp"
#include "config.hpp"
#include "socks.hpp"
#include "utils.hpp"


namespace ac
{
	template <typename Executor>
	struct proxy
		: public std::enable_shared_from_this<proxy<Executor>>
	{
		using executor_type = Executor;
		using stream_type = boost::asio::ip::tcp::socket;
		using stream_ptr_type = std::shared_ptr<stream_type>;
		using resolver_type = boost::asio::ip::tcp::resolver;
		using resolver_ptr_type = std::shared_ptr<resolver_type>;
		using timer_type = boost::asio::steady_timer;
		using timer_ptr_type = std::shared_ptr<timer_type>;
		using cs_ptr_type = std::shared_ptr<net::cancellation_signal>;

		enum ptype {
			socks,
			direct
		};

		struct ups
		{
			ptype pt;
			stream_ptr_type stream;
			timer_ptr_type timer;
			resolver_ptr_type resolver;
			upstream_conf_t upc;
			bool connected;
			bool handshake;
			cs_ptr_type cancel_signal;
		};
		using ups_ptr_type = std::shared_ptr<ups>;

		Executor ex_;
		config_t conf_;
		socks::req_t req_;
		std::vector<ups_ptr_type> ups_;
		stream_ptr_type stream_;
		std::once_flag first_arrival_;

		explicit proxy(Executor& ex, const config_t& conf, const socks::req_t& req)
			: ex_(ex)
			, conf_(conf)
			, req_(req)
		{
			if (!conf_.relay_only) {
				ups u;
				u.pt = direct;
				u.stream = std::make_shared<stream_type>(ex);
				u.timer = std::make_shared<timer_type>(ex);
				u.resolver = std::make_shared<resolver_type>(ex);
				u.connected = false;
				u.handshake = false;
				u.cancel_signal = std::make_shared<net::cancellation_signal>();
				ups_.emplace_back(std::make_shared<ups>(std::move(u)));
			}
			for (const auto& up : conf.upstreams) {
				ups u;
				u.pt = socks;
				u.stream = std::make_shared<stream_type>(ex);
				u.timer = std::make_shared<timer_type>(ex);
				u.resolver = std::make_shared<resolver_type>(ex);
				u.upc = up;
				u.connected = false;
				u.handshake = false;
				u.cancel_signal = std::make_shared<net::cancellation_signal>();
				ups_.emplace_back(std::make_shared<ups>(std::move(u)));
			}
		}
		~proxy() = default;

		executor_type get_executor() noexcept {
			return ex_;
		}

		boost::asio::awaitable<bool> connect_upstream() {
			namespace net = boost::asio;
			using namespace boost::asio::experimental::awaitable_operators;

			auto ex = get_executor();

			for (auto& u : ups_) {
				auto host = u->pt == ptype::socks ? u->upc.host : socks::to_string(req_.addr);
				auto port = u->pt == ptype::socks ? u->upc.port : req_.port;
				auto remote = host + ":" + std::to_string(port);
				try {
					u->timer->expires_after(std::chrono::seconds(1));

					LOG_DBG << "connect to " << remote;

					std::variant<net::ip::tcp::resolver::results_type, std::monostate> results = co_await (
						u->resolver->async_resolve(host, std::to_string(port), net::use_awaitable) 
						|| u->timer->async_wait(net::use_awaitable)
					);
					if (results.index() != 0) {
						continue;
					}

					u->timer->expires_after(std::chrono::seconds(2));
					std::variant<net::ip::tcp::endpoint, std::monostate> ret = co_await(
						net::async_connect(*(u->stream), std::get<0>(results), net::use_awaitable)
						|| u->timer->async_wait(net::use_awaitable)
					);
					if (ret.index() != 0) {
						LOG_WARN << "connect to " << host << " port " << port << " timeout";
						continue;
					}

					u->connected = true;
					LOG_INFO << "connect to " << remote << " done";

					boost::system::error_code ig;
					u->stream->set_option(net::ip::tcp::no_delay(true), ig);
				} catch (boost::system::system_error& error) {
					LOG_WARN << "connect to " << remote << " error: " << error.what();
				} catch (std::exception& ex) {
					LOG_WARN << "connect to " << remote << " unknown error: " << ex.what();
				}
			}
			std::erase_if(ups_, [](auto u){
				return !u->connected;
			});

			co_return !ups_.empty();
		}

		boost::asio::awaitable<bool> handshake() {
			using boost::asio::experimental::promise;
			using boost::asio::experimental::use_promise;

			auto ex = co_await boost::asio::this_coro::executor;

			std::vector<promise<void(std::exception_ptr)>> promises;
			for (auto& u : ups_) {
				if (u->pt == ptype::direct) {
					continue;
				}
				auto p = boost::asio::co_spawn(
					ex, 
					[this, u]() mutable -> boost::asio::awaitable<void> {
						socks::req_info_t req;
						req.req = req_;
						req.username = u->upc.username;
						req.password = u->upc.password;
						auto ec = co_await socks::async_socks_connect(*(u->stream), req);
						auto endp = u->stream->remote_endpoint();
						if (ec) {
							LOG_ERR << "failed to do handshake to upstream: " << utils::to_string(endp)
								<< ec.message();

							u->stream->close(ec);
						}
						u->handshake = true;
						co_return;
					}, 
					use_promise);
				promises.push_back(std::move(p));
			}
			for (auto& p : promises) {
				co_await p.async_wait(boost::asio::use_awaitable);
			}
			// remove closed
			std::erase_if(ups_, [](auto u){
				if (u->pt == ptype::direct) return false;
				return !u->stream->is_open() || !u->handshake;
			});

			co_return !ups_.empty();
		}

		template <typename Buffer, typename CompletionToken>
		auto async_read_some(const Buffer& buffer, CompletionToken&& token) {
			auto self(this->shared_from_this());
			auto init = [this, self, &buffer](auto&& handler) mutable {
				if (!stream_) {
					using hanlder_type = std::decay_t<decltype(handler)>;
					auto handler_ptr = std::make_shared<hanlder_type>(std::move(handler));

					auto read_cd = std::make_shared<int>(static_cast<int>(ups_.size()));
					for (auto& u : ups_) {
						auto sbuf = std::make_shared<socks::buffet_t>();
						u->stream->async_read_some(
							boost::asio::buffer(*sbuf),
							net::bind_cancellation_slot(
								u->cancel_signal->slot(),
								[this, self, u, sbuf, &buffer, handler_ptr, read_cd]
								(boost::system::error_code ec, std::size_t bytes_transfered) mutable {
									if (ec) {
										(* read_cd)--;
										auto endp = u->stream->remote_endpoint();

										{
											boost::system::error_code ec;
											u->stream->cancel(ec); // cancel all asynchronous operations
											u->stream->close(ec);
										}

										if (*read_cd == 0) {
											LOG_WARN << "all read operation failed, callback with error";
											(*handler_ptr)(ec, bytes_transfered);
										}

										return;
									}
									
									std::call_once(
										first_arrival_, 
										[this, self, u, sbuf, &buffer, ec, bytes_transfered, handler_ptr]() mutable {
											stream_ = u->stream;
											auto endp = u->stream->remote_endpoint();
											LOG_INFO << "chosen upstream: " << utils::to_string(endp);
											{
												// cancel others
												boost::system::error_code ig;
												for (auto& it : ups_) {
													if (it != u) {
														u->cancel_signal->emit(net::cancellation_type::total);
													}
												}
											}

											boost::asio::buffer_copy(buffer, boost::asio::buffer(*sbuf, bytes_transfered));
											(*handler_ptr)(ec, bytes_transfered);
										}
									);
								}
							)
						);
					}
				} else {
					return stream_->async_read_some(buffer, std::move(handler));
				}
			};

			return boost::asio::async_initiate<
				CompletionToken, void(boost::system::error_code, std::size_t)>(
					init, token);
		}

		template <typename Buffer, typename CompletionToken>
		auto async_write_some(const Buffer& buffer, CompletionToken&& token) {
			auto self(this->shared_from_this());
			auto init = [this, self, &buffer](auto&& handler) mutable {
				if (!stream_) {
					using hanlder_type = std::decay_t<decltype(handler)>;
					auto handler_ptr = std::make_shared<hanlder_type>(std::move(handler));

					auto buf_size = net::buffer_size(buffer);
					auto write_cd = std::make_shared<int>(static_cast<int>(ups_.size()));
					for (auto& u : ups_) {
						auto endp = u->stream->remote_endpoint();
						u->stream->async_write_some(
							buffer, 
							[
								this, self, u, handler_ptr, buf_size, write_cd
							](boost::system::error_code ec, std::size_t bytes_transfered) mutable {
								(*write_cd)--;
								// just ignore all the error

								if (*write_cd == 0) {
									// callback when all stream has been writen
									(*handler_ptr)({}, buf_size);
								}
							});
					}
				} else {
					return stream_->async_write_some(buffer, std::move(handler));
				}
			};

			return boost::asio::async_initiate<
				CompletionToken, void(boost::system::error_code, std::size_t)>(
					init, token);
		}
	};
}
