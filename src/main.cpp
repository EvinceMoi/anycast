
#include <cstdlib>
#include <iostream>
#include <optional>
#include <vector>

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>
#include <boost/program_options.hpp>

#include "config.hpp"
#include "io_context_pool.hpp"
#include "node.hpp"

boost::asio::awaitable<void> run(ac::io_context_pool& ios, ac::config_t config)
{
	auto ex = co_await boost::asio::this_coro::executor;
	boost::asio::signal_set sigs(ex);
	sigs.add(SIGINT);
	sigs.add(SIGTERM);

	ac::node node(ios, std::move(config));

	using namespace boost::asio::experimental::awaitable_operators;
	co_await(node.start() || sigs.async_wait(boost::asio::use_awaitable));
	node.stop();

	sigs.clear();
}

int main(int argc, char* argv[])
{

	namespace po = boost::program_options;

	std::string config_file;

	po::options_description desc("options");
	desc.add_options()
		("help,h", "show help messages")
		("version,v", "show version info")
		("config,c", po::value(&config_file), "config file path")
	;

	po::variables_map vm;
	po::store(po::parse_command_line(argc, argv, desc), vm);
	po::notify(vm);

	if (vm.count("help")) {
		std::cout << desc << std::endl;
		return EXIT_SUCCESS;
	}
	if (vm.count("version")) {
		std::cout << "anycast version 0.1" << std::endl;
		return EXIT_SUCCESS;
	}

	if (!vm.count("config")) {
		std::cerr << "no config files, exit now" << std::endl;
		return EXIT_FAILURE;
	}

	auto config = ac::parse_config(config_file);
	if (!config.has_value()) {
		return EXIT_FAILURE;
	}

	int exit_code = EXIT_SUCCESS;

	ac::io_context_pool ios{ 2 };

	boost::asio::co_spawn(ios.get_init_io(),
		run(ios, config.value()),
		[&](std::exception_ptr e)
		{
			if (e) {
				exit_code = EXIT_FAILURE;
				std::rethrow_exception(e);
			}

			ios.stop();
		});

	ios.run();

	return exit_code;
}