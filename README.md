# l_async
A Lightweight C++11 library for asynchronous programming


## Example

Suppose need to calc the total size of all files in the given subdirectories using the provided file system API:

```C++
template <typename T>
struct async_stream {
	virtual ~async_stream() = default;
	virtual void next(function<void(unique_ptr<T>)> callback) = 0;
};

struct async_file {
	virtual ~async_file() = default;
	virtual void get_size(function<void(int)> callback) const = 0;
};

struct async_dir {
	virtual ~async_dir() = default;
	virtual unique_ptr<async_stream<async_file>> get_files() const = 0;
	virtual unique_ptr<async_stream<async_dir>> get_dirs() const = 0;
};
```

All we need to do is traverse the lists of files and directories, acquires file sizes and calculate the total size:

```C++
void calc_tree_size_async(const async_dir& root, function<void(int)> callback);
```

This API is async, that allows us to speed-up our tasks because our thread doesn't have to wait on `next`/`get size` blocking calls, and even more, it allows us to traverse many subdirectories in parallel, but on the other hand our async code will be very tricky and cumbersome.
- Our data structures have to preserve `async_stream` instances across asynchronous iterations on `next` calls.
- We have to support nested recursive or parallel co-existing iteration contexts with data and results.
- We need to organize some reactive result delivery and accounting when to notificate callbacks.
- Unique_ptrs are not copy-constructible, thus std::functions, should we elect to use them, can't store these pointers in their capture blocks.

Overall, is it hard to write such code?
You can stop reading here and try to make your own solution first.

This is my solution using `l_async`:

```C++
#include "l_async.h"
using l_async::loop;
using l_async::result;
using l_async::unique;

void calc_tree_size_async(const async_dir& root, result<int> result) {
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
				result.data() += size;
			});
			next();
		});
	});
}

void calc_tree_size_async(const async_dir& root, function<void(int)> callback) {
	calc_tree_size_async(root, result<int>(callback));
}
```

This solution:
- has about the same size as in synchronous case,
- has same structure and same complexity,
- and even more, it performs scan in parallel,
- and it is protected against stack overflow in the case the if `async_stream::next` calls its callbacks synchronously.

Detailed comparison of sync and async code can be found [here](https://github.com/karol11/l_async/blob/main/docs/)

## How it works

The entire library consists of just three primitives:
- `l_async::unique<T>`
- `l_async::result<T>`
- `l_async::loop`

### `l_async::unique`

C++ language designers should have supported move-only lambdas capturing move-only data types. But they didn't. That's what `unique` is for: it wraps data type and tells the compiler that this type is now copy-constructible, but it fails on assert on attempt to copy data. Of course, this wrapper should be used only in lsmbdas that are move-only by design. Luckily `l_async::loop` and `l_async::result`  guarantee that their lambdas will never be copied.

So it is a transparent wrapper for any type. It supports move semantics, disallows assignments and terminates programs on attempts to copy data. It's useful when we need to capture `std::unique_ptr` in lambdas.

In the above example it is used to store file and dir streams between get_next iterations.

### `l_async::result<T>`

It's a shared_ptr to a memory block, that holds data of a given type along with a callback that accepts this type as parameter.
* You can freely pass this object by value and store it in any levels of your processing lambdas.
* You can access and modify this data.
* At the moment it is no longer referenced, it calls its callback with its data.

You can think of it as the less limiting generalization of `promise_all` pattern in other languages.

It can be used to combine data from different branches of asynchronous processes.
Tree of `l_async::result`-s provides reactive data and control transfers for processes having subprocesses.

### `l_async::loop`

It's a workhorse of this library. It organizes the asybchronous iterative processes.
It accepts lambda which capture block represents context data, that should be preserved across iterations and code is a loop body.
- Initially `l_async::loop` moves its lambda to the heap-allocated shared block. This guarantees that captured objects will never be copied.
- Then it calls this lambda passing to it l_async::loop instance (itself) as parameter (who said Y-combinator?).

From the data lifetime perspective in loop body we have direct access to the context data and to the `next` object, that upholds this context data.
From the control flow perspective, the first call of lambda is performed synchronously at the `loop` creation. The `next` paramter not only controls
context data lifetime, it also it can be called to perform the next loop iteration.

Our loop body lambda can use its `next` parameter in four ways:
- Ignore it; this breaks the loop and destroys the context.
- Or pass it to some function, that expects `std::function<void()>` to be called later; this prolongs lifetime of the context data and allows asynchronous iterations.
- Or capture it in some callback, that expects data and call later from that callback as shown in above example, this is also produces asynchronous iterations.
- Or call it synchronously or pass to a function that will call it synchronously; this sets a flag, that will restart the lambda immediately after it returned, performing iteration without stack overflow. BTW, it might be this case in the above example, if `stream->next` will call its callback synchrously.

## Structure
- `include/l_async.h` - single header library itself,
- `docs/*` - sync and async examples mentiomed in this readme,
- `examples/*` - more detailed per-primitive examples,
- `tests/single_thread_executor.h` - helper class to imitate asynchronous framework for test and examples,
- `tests/gunit.*` - lightweight testing framework, that mimics the very basic parts of GUNIT (just to avoid external depts),
- `tests/*` - other tests,
- `CMakeLists.txt` - builds test and examples.
