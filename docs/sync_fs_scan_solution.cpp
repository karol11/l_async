#include "sync_fs_scan_problem.h"

int calc_tree_size_sync(const sync_dir& root)
{
    int result = 0;
    auto stream_of_dirs = root.get_dirs();
    unique_ptr<sync_dir> sub_dir;
    while ((sub_dir = stream_of_dirs->next()))
    {
        result += calc_tree_size_sync(*sub_dir);
    }
    auto stream_of_files = root.get_files();
    unique_ptr<sync_file> file;
    while ((file = stream_of_files->next()))
    {
        result += file->get_size();
    }
    return result;
}
