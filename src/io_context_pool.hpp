
#pragma once

#include <boost/asio.hpp>

namespace ac
{
	class io_context_pool
	{
		using context_ptr = std::shared_ptr<boost::asio::io_context>;
		using work_t = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;
		using work_ptr = std::shared_ptr<work_t>;
	private:
		std::size_t psize_;
		std::size_t idx_;

		boost::asio::io_context init_io_;

		std::vector<context_ptr> pool_;
		std::vector<work_ptr> work_;
	private:
		io_context_pool(const io_context_pool&) = delete;
		io_context_pool& operator=(const io_context_pool&) = delete;
	public:
		explicit io_context_pool(std::size_t size);
		~io_context_pool();

		operator boost::asio::io_context& ();
	public:
		void run();
		void stop();

		boost::asio::io_context& get_io();
		boost::asio::io_context& get_init_io();

		std::size_t pool_size() const;
	};
}