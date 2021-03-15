#ifndef _ASYNC_FS_SCAN_PROBLEM_H_
#define _ASYNC_FS_SCAN_PROBLEM_H_

#include <functional>
using std::function;

#include <memory>
using std::unique_ptr;

template <typename T>
struct async_stream
{
	virtual ~async_stream() = default;
	virtual void next(function<void(unique_ptr<T>)> callback) = 0;  // calls callback with nullptr on list end
};

struct async_file
{
	virtual ~async_file() = default;
	virtual void get_size(function<void(int)> callback) const = 0;
};

struct async_dir
{
	virtual ~async_dir() = default;
	virtual unique_ptr<async_stream<async_file>> get_files() const = 0;
	virtual unique_ptr<async_stream<async_dir>> get_dirs() const = 0;
};

//
// Implement this:
//
void calc_tree_size_async(const async_dir& root, function<void(int)> callback);

#endif // _ASYNC_FS_SCAN_PROBLEM_H_
