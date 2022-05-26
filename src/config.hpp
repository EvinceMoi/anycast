#pragma once

#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <fstream>
#include <optional>

#include "logging.hpp"

namespace ac {

	struct upstream_conf_t
	{
		// socks5 config
		std::string host;
		uint16_t port;
		std::string username;
		std::string password;
	};
	struct config_t
	{
		bool relay_only; // default to false
		std::vector<std::string> bind_addrs;
		std::vector<upstream_conf_t> upstreams;
	};

	inline std::optional<config_t> parse_config(const std::string& config_file)
	{
		namespace fs = boost::filesystem;
		boost::system::error_code ec;
		if (!fs::exists(config_file, ec)) {
			std::cerr << "config file not exist, " << ec.message() << std::endl;
			return {};
		}

		std::ifstream cf(config_file, std::ios::in);
		std::string content;
		cf.seekg(0, std::ios::end);
		content.reserve(cf.tellg());
		cf.seekg(0, std::ios::beg);
		content.assign(std::istreambuf_iterator<char>(cf), std::istreambuf_iterator<char>());
		LOG_INFO << "config file content: " << content;

		boost::json::parse_options opt;
		opt.allow_comments = true;
		auto jv			   = boost::json::parse(content, ec, {}, opt);
		if (ec) {
			LOG_ERR << "config parse error: " << ec.message();
			return {};
		}

		if (!jv.is_object()) {
			LOG_ERR << "config file format error. ";
			return {};
		}

		auto root = jv.as_object();
		/*
		 * listen: [ "[::]: 9099", ... ]
		 * upstreams: [{ type, ... }]
		 */

		std::string error;
		do {
			config_t ret;

			ret.relay_only = false;
			if (root.contains("relay_only")) {
				auto ro = root.at("relay_only");
				if (ro.is_bool()) {
					ret.relay_only = ro.as_bool();
				}
			}
			std::cout << std::boolalpha << "relay_only mode: " << ret.relay_only << std::endl;

			std::vector<std::string> lis;
			if (root.contains("listen")) {
				auto listen = root.at("listen");
				if (listen.is_array() && listen.as_array().size() > 0) {
					auto listen_arr = listen.as_array();
					error			= "listen";

					for (auto const& li : listen_arr) {
						if (!li.is_string())
							goto parse_fail;
						lis.push_back(boost::json::value_to<std::string>(li));
					}
				}
			}
			if (lis.empty()) {
				std::string default_listen_addr = "0.0.0.0:10203";
				LOG_INFO << "no valid listen config, use default listen addr: " << default_listen_addr;
				lis.push_back(default_listen_addr);
			}
			ret.bind_addrs = lis;

			std::vector<upstream_conf_t> ups;
			if (root.contains("upstreams")) {
				auto upstreams = root.at("upstreams");
				if (!upstreams.is_array() || upstreams.as_array().size() == 0) {
					//error = "upstream";
					//break;
				} else {
					auto upsteam_arr = upstreams.as_array();
					error			 = "upstream";
					for (auto const& upv : upsteam_arr) {
						if (!upv.is_object())
							goto parse_fail;

						auto up = upv.as_object();
						upstream_conf_t u;

						if (!up.contains("host") || !up.at("host").is_string())
							goto parse_fail;
						auto const& host = up.at("host");
						u.host			 = boost::json::value_to<std::string>(host);

						if (!up.contains("port") || !up.at("port").is_int64())
							goto parse_fail;
						auto port = up.at("port");
						u.port	  = static_cast<uint16_t>(port.as_int64());

						if (up.contains("username")) {
							auto const& username = up.at("username");
							if (username.is_string())
								u.username = boost::json::value_to<std::string>(username);
						}

						if (up.contains("password")) {
							auto const& password = up.at("password");
							if (password.is_string())
								u.password = boost::json::value_to<std::string>(password);
						}

						ups.emplace_back(std::move(u));
					}
				}
			}

			//if (ups.empty())
			//	LOG_INFO << "no valid upstream config";

			ret.upstreams = std::move(ups);
			return ret;
		} while (false);

	parse_fail:
		LOG_ERR << "config file error, please check " << error;
		return {};
	}
}