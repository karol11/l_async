#include "sync_fs_scan_problem.h"
#include "gunit.h"

namespace
{
	using std::make_unique;

	template<typename INTERFACE, typename IMPL>
	struct fake_sync_stream : sync_stream<INTERFACE>
	{
		int left, param;
		fake_sync_stream(int left, int param)
			: left(left)
			, param(param)
		{}

		unique_ptr<INTERFACE> next() override
		{
			return left > 0
				? --left, make_unique<IMPL>(param)
				: unique_ptr<IMPL>();
		}
	};

	struct fake_sync_file : sync_file
	{
		int size;
		fake_sync_file(int size)
			: size(size)
		{}

		int get_size() const override
		{
			return size;
		}
	};

	struct fake_sync_dir : sync_dir
	{
		int depth;
		fake_sync_dir(int depth)
			: depth(depth)
		{}

		unique_ptr<sync_stream<sync_file>> get_files() const override
		{
			// The number of files in the fake dir is the depth of this dir, and so their sizes
			return make_unique<fake_sync_stream<sync_file, fake_sync_file>>(depth, depth);
		}

		unique_ptr<sync_stream<sync_dir>> get_dirs() const override
		{
			// The number of dirs countdown from 3 to zero with descending to subdirectories
			return make_unique<fake_sync_stream<sync_dir, fake_sync_dir>>(3 - depth, depth + 1);
		}
	};

	TEST(LAsync, FileSystemAsyncTest)
	{
		ASSERT_EQ(
			calc_tree_size_sync(fake_sync_dir{ 0 }),
			81);
	}
}
