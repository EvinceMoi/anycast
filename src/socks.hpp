#pragma once

#include <variant>

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/system/error_code.hpp>

#include "types.hpp"
#include "buffer.hpp"
#include "logging.hpp"

namespace ac::socks {

	// https://datatracker.ietf.org/doc/html/rfc1928
	enum handshake_type
	{
		client,
		server
	};

	enum version
	{
		v5 = 0x05,
	};

	enum method_type
	{
		no_auth				  = 0x00,
		//gssapi				  = 0x01,
		usrname_password	  = 0x02,
		no_acceptable_methods = 0xff
	};

	enum cmd
	{
		connect		  = 0x01,
		bind		  = 0x02,
		udp_associate = 0x03
	};

	enum atyp // address type
	{
		ipv4   = 0x01,
		domain = 0x03, // 1 byte length followed by domain name
		ipv6   = 0x04
	};

	namespace error {

		enum errc_t
		{
			succeeded				   = 0x00,
			general_server_failure	   = 0x01,
			connection_not_allowed	   = 0x02,
			network_unreachable		   = 0x03,
			host_unreachable		   = 0x04,
			connection_refused		   = 0x05,
			ttl_expired				   = 0x06,
			command_not_supported	   = 0x07,
			address_type_not_supported = 0x08,

			invalid_version = 0x30,
			invalid_payload = 0x31,
			no_acceptable_methods = 0x32,
		};

		class socks_category : public boost::system::error_category
		{
		public:
			const char* name() const noexcept override { return "socks"; }
			std::string message(int ev) const override
			{
				switch (ev) {
					case succeeded:
						return "succeeded";
					case general_server_failure:
						return "general server failure";
					case connection_not_allowed:
						return "connection not allowed";
					case network_unreachable:
						return "network unreachable";
					case host_unreachable:
						return "host unreachable";
					case connection_refused:
						return "connection refused";
					case ttl_expired:
						return "ttl expired";
					case command_not_supported:
						return "command not supported";
					case address_type_not_supported:
						return "address type not supported";
					case invalid_version:
						return "version invalid";
					case invalid_payload:
						return "payload invalid";
				}

				return "socks error";
			}
		};

		inline const boost::system::error_category& error_category()
		{
			static socks_category cat;
			return cat;
		}

		inline boost::system::error_code make_error_code(errc_t e)
		{
			return boost::system::error_code(static_cast<int>(e), error_category());
		}
	}
}

namespace boost::system {
	template <>
	struct is_error_code_enum<ac::socks::error::errc_t> : std::true_type
	{
	};
}

namespace ac::socks {

	using accept_ret_t = std::tuple<boost::system::error_code, req_t>;
	//struct connect_opt
	//{
	//	address_type addr;
	//	uint16_t port;
	//	std::string username;
	//	std::string password;
	//};

	namespace detail {

		template <typename AsyncStream>
		boost::asio::awaitable<boost::system::error_code> socks_connect(AsyncStream& stream, const req_info_t& opt)
		{
			namespace net = boost::asio;
			using net::use_awaitable;

			try {
				net::streambuf buf;
				{ // auth method negotiation
					auto it = static_cast<uint8_t *>(buf.prepare(3).data());
					buffer::write<uint8_t>(it, version::v5);
					buffer::write<uint8_t>(it, 0x01); // NMETHODS
					auto method = (!opt.username.empty() || !opt.password.empty()) ? method_type::usrname_password : method_type::no_auth;
					buffer::write<uint8_t>(it, method);
					buf.commit(3);

					auto bytes = co_await net::async_write(stream, buf, use_awaitable);

					bytes = co_await net::async_read(stream, buf, net::transfer_exactly(2), use_awaitable);
					auto p = static_cast<const char*>(buf.data().data());
					auto version = buffer::read<uint8_t>(p);
					auto selected_method = buffer::read<uint8_t>(p);
					buf.consume(2);

					if (version != v5) {
						throw boost::system::system_error(error::invalid_version);
					}
					if (selected_method == method_type::no_acceptable_methods) {
						throw boost::system::system_error(error::no_acceptable_methods); // client should close connection
					}
					if (selected_method == method_type::usrname_password) {
						// do auth negotiation
					}
				}
				{ // request
					/**
					 *  +----+-----+-------+------+----------+----------+
						|VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
						+----+-----+-------+------+----------+----------+
						| 1  |  1  | X'00' |  1   | Variable |    2     |
						+----+-----+-------+------+----------+----------+
					*/
					auto max_len = 1 + 1 + 1 + 1 + 1 + std::numeric_limits<uint8_t>::max() + 2;
					auto it = static_cast<uint8_t *>(buf.prepare(max_len).data());
					auto start = it;
					buffer::write<uint8_t>(it, version::v5); // VER
					buffer::write<uint8_t>(it, cmd::connect); // CMD
					buffer::write<uint8_t>(it, 0x00); // RSV
					auto atyp = atyp::domain;
					if (std::holds_alternative<net::ip::address>(opt.req.addr)) {
						auto addr = std::get<net::ip::address>(opt.req.addr);
						if (addr.is_v4()) {
							atyp = atyp::ipv4;
							buffer::write<uint8_t>(it, atyp);
							for (auto const& byte : addr.to_v4().to_bytes())
								buffer::write<uint8_t>(it, byte);
						} else {
							atyp = atyp::ipv6;
							buffer::write<uint8_t>(it, atyp);
							for (auto const& byte : addr.to_v6().to_bytes())
								buffer::write<uint8_t>(it, byte);
						}
					} else {
						auto addr = std::get<std::string>(opt.req.addr);
						auto atyp = atyp::domain;
						buffer::write<uint8_t>(it, atyp);
						buffer::write<uint8_t>(it, static_cast<uint8_t>(addr.size()));
						buffer::write_string(it, addr);
					}
					buffer::write<uint16_t>(it, opt.req.port);
					auto buf_size = std::distance(start, it);
					buf.commit(buf_size);

					auto bytes = co_await net::async_write(stream, buf, use_awaitable);

					bytes = co_await net::async_read(stream, buf, net::transfer_exactly(2), use_awaitable);
					auto p = static_cast<const char*>(buf.data().data());
					auto version = buffer::read<uint8_t>(p);
					auto rep = buffer::read<uint8_t>(p);
					buf.consume(2);

					if (version != v5) {
						throw boost::system::system_error(error::invalid_version);
					}
					if (rep != error::succeeded) {
						auto errc = error::invalid_payload;
						if (rep < 0x09) {
							errc = static_cast<error::errc_t>(rep);
						}
						throw boost::system::system_error(errc);
					}

					bytes = co_await net::async_read(stream, buf, net::transfer_exactly(2), use_awaitable);
					{
						auto p = static_cast<const char*>(buf.data().data());
						buffer::read<uint8_t>(p); // RSV
						auto atyp = buffer::read<uint8_t>(p);

						std::size_t addrlen = 0;
						if (atyp == 0x01) {
							addrlen = 4;
						} else if (atyp == 0x04) {
							addrlen = 16;
						} else if (atyp == 0x03) {
							co_await net::async_read(stream, buf, net::transfer_exactly(1), use_awaitable);

							auto p	 = static_cast<const char*>(buf.data().data());
							auto len = buffer::read<uint8_t>(p);
							buf.consume(1);
							addrlen = len;
						} else {
							// atyp error
							throw boost::system::system_error(error::invalid_payload);
						}

						co_await net::async_read(stream, buf, net::transfer_exactly(addrlen + 2), use_awaitable); // addr + port

						req_addr_t dst_addr;
						uint16_t dst_port;
						{
							if (atyp == 0x01 || atyp == 0x04) {
								// ipv4 / ipv6
								if (atyp == 0x01) {
									net::ip::address_v4::bytes_type bytes;
									for (auto i = 0; i < 4; i++) {
										bytes[i] = buffer::read<uint8_t>(p);
									}
									dst_addr = net::ip::make_address_v4(bytes);
								} else {
									net::ip::address_v6::bytes_type bytes;
									for (auto i = 0; i < 16; i++) {
										bytes[i] = buffer::read<uint8_t>(p);
									}
									dst_addr = net::ip::make_address_v6(bytes);
								}
							} else {
								// domain
								dst_addr = buffer::read_string(p, addrlen);
							}

							dst_port = buffer::read<uint16_t>(p);
						}
					}

					// got server bound address and port
					co_return boost::system::error_code{};
				}

			} catch (boost::system::system_error& e) { 
				co_return e.code();
			}
		}


		template <typename AsyncStream>
		boost::asio::awaitable<accept_ret_t> socks_accept(AsyncStream& stream)
		{
			namespace net = boost::asio;
			using net::use_awaitable;

			try {

				boost::system::error_code ec;
				net::streambuf buf;
				{ // auth method negotiation
					auto bytes = co_await net::async_read(stream, buf, net::transfer_exactly(2), use_awaitable);
					auto p = static_cast<const char*>(buf.data().data());
					auto version = buffer::read<uint8_t>(p);
					if (version != v5) {
						throw boost::system::system_error(error::invalid_version);
					}

					auto nmethods = buffer::read<uint8_t>(p);
					if (nmethods == 0) {
						throw boost::system::system_error(error::invalid_payload);
					}
					buf.consume(2);

					bytes = co_await net::async_read(stream, buf, net::transfer_exactly(nmethods), use_awaitable);
					buf.consume(nmethods);

					// force use 0x00, no authentication required
					auto it = static_cast<uint8_t *>(buf.prepare(2).data());
					buffer::write<uint8_t>(it, version::v5);
					buffer::write<uint8_t>(it, method_type::no_auth);
					buf.commit(2);
					bytes = co_await net::async_write(stream, buf, use_awaitable);
				}

				{	// requests
					/**
					 *  +----+-----+-------+------+----------+----------+
						|VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
						+----+-----+-------+------+----------+----------+
						| 1  |  1  | X'00' |  1   | Variable |    2     |
						+----+-----+-------+------+----------+----------+
					*/
					co_await net::async_read(stream, buf, net::transfer_exactly(4), use_awaitable);

					auto p = static_cast<const char*>(buf.data().data());
					buffer::read<uint8_t>(p); // VER
					auto cmd = buffer::read<uint8_t>(p); // CMD
					//if (cmd != 0x01 && cmd != 0x03)
					if (cmd != 0x01) // only CONNECT for now
						throw boost::system::system_error(error::command_not_supported); // 01: connect, 02: bind, 03: udp associate

					buffer::read<uint8_t>(p); // RSV
					auto atyp = buffer::read<uint8_t>(p); // ATYP
					buf.consume(4);

					std::size_t addrlen = 0;
					if (atyp == 0x01) {
						addrlen = 4;
					} else if (atyp == 0x04) {
						addrlen = 16;
					} else if (atyp == 0x03) {
						co_await net::async_read(stream, buf, net::transfer_exactly(1), use_awaitable);

						auto p	 = static_cast<const char*>(buf.data().data());
						auto len = buffer::read<uint8_t>(p);
						buf.consume(1);
						addrlen = len;
					} else {
						// atyp error
						throw boost::system::system_error(error::invalid_payload);
					}

					co_await net::async_read(stream, buf, net::transfer_exactly(addrlen + 2), use_awaitable); // addr + port
					
					req_addr_t dst_addr;
					uint16_t dst_port;
					{
						auto p = static_cast<const char*>(buf.data().data());

						if (atyp == 0x01 || atyp == 0x04) {
							// ipv4 / ipv6
							if (atyp == 0x01) {
								net::ip::address_v4::bytes_type bytes;
								for (auto i = 0; i < 4; i++) {
									bytes[i] = buffer::read<uint8_t>(p);
								}
								dst_addr = net::ip::make_address_v4(bytes);
							} else {
								net::ip::address_v6::bytes_type bytes;
								for (auto i = 0; i < 16; i++) {
									bytes[i] = buffer::read<uint8_t>(p);
								}
								dst_addr = net::ip::make_address_v6(bytes);
							}
						} else {
							// domain
							dst_addr = buffer::read_string(p, addrlen);
						}

						dst_port = buffer::read<uint16_t>(p);
						
						req_t req;
						req.addr = dst_addr;
						req.port = dst_port;

						// got dst_addr / dst_port
						LOG_INFO << "request: " << to_string(dst_addr) << ":" << dst_port;

						co_return accept_ret_t{ ec, req };
					}
				}

			} catch (boost::system::system_error& e) {
				co_return accept_ret_t{ e.code(), {} };
			}
		} 

		template <typename AsyncStream>
		boost::asio::awaitable<bool> socks_respond(AsyncStream& stream, const req_t& req, boost::system::error_code ec) {
			namespace net = boost::asio;
			using net::use_awaitable;
			try {
				net::streambuf buf;
				auto max_buf_size = 1 // ver
					+ 1  // rep
					+ 1  // rsv
					+ 1  // atyp
					+ 1  // addr len
					+ 256 // addr
					+ 2; // port
				auto it = static_cast<uint8_t *>(buf.prepare(max_buf_size).data());
				auto start = it;
				buffer::write<uint8_t>(it, version::v5); // ver
				buffer::write<uint8_t>(it, static_cast<uint8_t>(ec.value())); // rep
				buffer::write<uint8_t>(it, 0x00); // rsv
				if (req.addr.index() == 0) {
					// net::ip::addr
					auto addr = std::get<0>(req.addr);
					if (addr.is_v4()) {
						buffer::write<uint8_t>(it, atyp::ipv4);
						for (auto const& byte : addr.to_v4().to_bytes())
							buffer::write<uint8_t>(it, byte);
					} else {
						buffer::write<uint8_t>(it, atyp::ipv6);
						for (auto const& byte : addr.to_v6().to_bytes())
							buffer::write<uint8_t>(it, byte);
					}
				} else {
					// string
					auto addr = std::get<1>(req.addr);
					buffer::write<uint8_t>(it, atyp::domain);
					buffer::write<uint8_t>(it, addr.size());
					buffer::write_string(it, addr);
				}
				buffer::write<uint16_t>(it, req.port);

				auto buf_size = std::distance(start, it);
				buf.commit(buf_size);
				co_await net::async_write(stream, buf, use_awaitable);
				co_return true;
			} catch (boost::system::system_error& e) {
				LOG_ERR << "socks_repond failed: " << e.what();
				co_return false;
			}
		}
	}

	template<typename AsyncStream>
	boost::asio::awaitable<accept_ret_t> async_socks_accept(AsyncStream& stream) {
		return detail::socks_accept(stream);
	}
	template<typename AsyncStream>
	boost::asio::awaitable<boost::system::error_code> async_socks_connect(AsyncStream& stream, const req_info_t& req) {
		return detail::socks_connect(stream, req);
	}
	template<typename AsyncStream>
	boost::asio::awaitable<bool> async_socks_respond(AsyncStream& stream, const req_t& req, boost::system::error_code ec) {
		return detail::socks_respond(stream, req, ec);
	}
}