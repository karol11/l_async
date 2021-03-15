#include <memory>
using std::make_unique;

#include "async_fs_scan_problem.h"
#include "gunit.h"
#include "single_thread_executor.h"
using executor = testing::single_thread_executor;

namespace
{
	template<typename INTERFACE, typename IMPL>
	struct fake_async_stream : async_stream<INTERFACE>
	{
		int left, param;
		executor& ex;

		fake_async_stream(int left, int param, executor& ex)
			: left(left)
			, param(param)
			, ex(ex)
		{}

		void next(function<void(unique_ptr<INTERFACE>)> callback) override
		{
			ex.schedule([=, callback = move(callback)] {
				callback(left > 0
					? --left, make_unique<IMPL>(param, ex)
					: unique_ptr<IMPL>());
			});
		}
	};

	struct fake_async_file : async_file
	{
		int size;
		executor& ex;

		fake_async_file(int size, executor& ex)
			: size(size)
			, ex(ex)
		{}

		void get_size(function<void(int)> callback) const override
		{
			ex.schedule([size = size, callback = move(callback)]{
				callback(size);
			});
		}
	};

	struct fake_async_dir : async_dir
	{
		int depth;
		executor& ex;

		fake_async_dir(int depth, executor& ex)
			: depth(depth)
			, ex(ex)
		{}

		unique_ptr<async_stream<async_file>> get_files() const override
		{
			// The number of files in the fake dir is the depth of this dir, and so their sizes
			return make_unique<fake_async_stream<async_file, fake_async_file>>(depth, depth, ex);
		}

		unique_ptr<async_stream<async_dir>> get_dirs() const override
		{
			// The number of dirs countdown from 3 to zero with descending to subdirectories
			return make_unique<fake_async_stream<async_dir, fake_async_dir>>(3 - depth, depth + 1, ex);
		}
	};

	TEST(LAsync, FileSystemSyncTest)
	{
		executor ex;
		calc_tree_size_async(fake_async_dir{ 0, ex }, [](auto size) {
			ASSERT_EQ(size, 81);
		});
		ex.execute();
	}
}
