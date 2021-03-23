#ifndef _SYNC_FS_SCAN_PROBLEM_H_
#define _SYNC_FS_SCAN_PROBLEM_H_

#include <memory>
using std::unique_ptr;

template <typename T>
struct sync_stream
{
    virtual ~sync_stream() = default;
    virtual unique_ptr<T> next() = 0;  // returns nullptr on list end
};

struct sync_file
{
    virtual ~sync_file() = default;
    virtual int get_size() const = 0;
};

struct sync_dir
{
    virtual ~sync_dir() = default;
    virtual unique_ptr<sync_stream<sync_file>> get_files() const = 0;
    virtual unique_ptr<sync_stream<sync_dir>> get_dirs() const = 0;
};

//
// Implement this:
//
int calc_tree_size_sync(const sync_dir& root);

#endif // _SYNC_FS_SCAN_PROBLEM_H_
