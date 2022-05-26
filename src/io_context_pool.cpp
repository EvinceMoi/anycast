#include "io_context_pool.hpp"

namespace ac {
	io_context_pool::io_context_pool(std::size_t size)
		: psize_(size)
		, idx_(0)
	{
		std::size_t hc = std::thread::hardware_concurrency();
		psize_		   = std::clamp<std::size_t>(psize_, 1, hc + 2);

		for (std::size_t i = 0; i < psize_; i++) {
			context_ptr context(new boost::asio::io_context{1});
			pool_.push_back(context);
		}
	}

	io_context_pool::~io_context_pool() { }

	void io_context_pool::run()
	{
		using thread_ptr = std::shared_ptr<std::thread>;

		std::vector<thread_ptr> threads;

		for (auto const& ctx : pool_) {
			work_ptr work = std::make_shared<work_t>(std::move(boost::asio::make_work_guard(*ctx)));
			work_.push_back(work);

			thread_ptr thread(new std::thread([ctx]() { ctx->run(); }));
			threads.push_back(thread);
		}

		init_io_.run();

		for (auto const& thread : threads) {
			thread->join();
		}
	}

	void io_context_pool::stop()
	{
		for (auto& work : work_) {
			work.reset();
		}
		std::vector<work_ptr>().swap(work_);
	}

	boost::asio::io_context& io_context_pool::get_io()
	{
		auto& io = *pool_[idx_];
		idx_	 = (idx_ + 1) % psize_;
		return io;
	}

	boost::asio::io_context& io_context_pool::get_init_io() {
		return init_io_;
	}

	std::size_t io_context_pool::pool_size() const { return psize_; }

	io_context_pool::operator boost::asio::io_context&() { return get_io(); }
}