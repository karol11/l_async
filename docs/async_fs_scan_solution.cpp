#include "async_fs_scan_problem.h"
#include "l_async.h"
using l_async::loop;
using l_async::result;
using l_async::unique;

void calc_tree_size_async(const async_dir& root, result<int> result)
{
	loop dirs([=, stream = unique(root.get_dirs())](auto next) mutable {
		stream->next([&, next](auto dir) {
			if (!dir) return;
			calc_tree_size_async(*dir, result);
			next();
		});
	});
	loop files([=, stream = unique(root.get_files())](auto next) mutable {
		stream->next([&, next](auto file) {
			if (!file) return;
			file->get_size([=](int size) mutable {
				*result += size;
			});
			next();
		});
	});
}

void calc_tree_size_async(const async_dir& root, function<void(int)> callback)
{
	calc_tree_size_async(root, result<int>(callback));
}
