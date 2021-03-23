# l_async
A Lightweight C++11 library for asynchronous programming


## Example

Suppose we need to calc the total size of all files in the given tree of subdirectories using the provided file system API:

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

All we need to do is traverse the lists of files and directories, acquire file sizes and calculate the total size:

```C++
void calc_tree_size_async(const async_dir& root, function<void(int)> callback);
```

This API is async, that allows us to speed-up our tasks because our thread doesn't have to wait on `next`/`get size` blocking calls, and even more, it allows us to traverse many subdirectories in parallel, but on the other hand our async code will be very tricky and cumbersome:
- Our data structures have to preserve `async_stream` instances across asynchronous iterations of `async_stream::next` calls.
- We have to support the nested-recursive or parallel-co-existing iteration contexts with data and results.
- We need to organize some reactive result delivery and callbacks notification mechanisms.
- `std::unique_ptr`s are not copy-constructible, so `std::function`s, should we elect to use them, can't store these pointers in their capture blocks.

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
				*result += size;
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

The entire library consists of just four primitives:
- `l_async::unique<T>`
- `l_async::result<T>`
- `l_async::loop`
- `l_async::slot`

### `l_async::unique`

C++ language designers should have supported move-only lambdas capturing move-only data types. But they didn't. That's what `unique` is for: it wraps data type and lies to the compiler that this type is now copy-constructible, but it fails on assert on copy attempts. Of course, this wrapper should be used only in lambdas that are move-only by design. Luckily `l_async::loop` and `l_async::result`  guarantee that their lambdas will never be copied.

So it is a transparent wrapper for any type. It supports move semantics, disallows assignments and terminates programs on attempts to copy data. It's useful when we need to capture move-only objects (like `std::unique_ptr`) in lambdas.

In the above example it is used to store file and dir streams between get_next iterations.

### `l_async::result<T>`

It's a shared_ptr to a memory block, that holds data of a given type along with a callback that accepts this type as parameter.
* You can freely pass this object by value and store it in any levels of your processing lambdas.
* You can access and modify this data.
* At the moment it is no longer referenced, it calls its callback with its data.

You can think of it as the less limiting generalization of `promise_all` pattern in other languages.

It can be used to combine data from different branches of asynchronous processes.
Tree of `l_async::result`-s provides reactive data and control transfers for processes having subprocesses.

It is useful to organize the parallel loops and combine the parallel results of different processes.

### `l_async::loop`

It's a workhorse of this library. It organizes the asynchronous iterative processes.
It accepts a lambda that captures the data that should be preserved across iterations; the lambda body becomes the loop body.
- First the `l_async::loop` moves its lambda to a heap-allocated shared block. This guarantees that captured objects will never be copied.
- Then it calls this lambda passing to it a shared pointer to this block as a parameter (who said Y-combinator?).

From the data lifetime perspective: In the loop body we have the direct access to the context data and to the `next` object, that upholds this context data.
From the control flow perspective: The first call to the passed lambda is performed synchronously at the `loop` creation. The `next` parameter not only controls
context data lifetime, it also can be called to perform the next loop iteration.

Overal, `l_async::loop` is a `std::function<void()>` and also it's a shared pointer to lambda and all its captures.\
All `auto next` parameters in the above example is of type  `l_async::loop`. 

Our loop body lambda can use its `next` parameter in four ways:
- Ignore it; this breaks the loop and destroys the context.
- Or pass it to some function, that expects `std::function<void()>` to be called later; this prolongs lifetime of the context data and allows asynchronous iterations.
- Or capture it in some callback, that expects data and call later from that callback as shown in the above example, this is also produces asynchronous iterations.
- Or call it synchronously or pass to a function that will call it synchronously; this also initiates the new iteration but in slightly different manner: it sets a flag, that will make `l_async::loop` to restart the lambda immediately after it returned, performing iteration without stack overflow. BTW, it might be this case in the above example, if `stream->next` will call its callback synchrously.

### `l_async::slot`

Async data processing often uses the concept of data providers and data consumers.
Generalized data consumer is a callback that accepts data: `function<void(T)> callback`. With the help of `l_async` it is very easy to write data consumers that request data sequentially (`l_async::loop`), and/or in parallel (`l_async::result`). Consumer can have own callbacks and call another consumers, thus consumers are combinable.

The providers are a little bit more trickiy.
- Generalized provider accepts a request for data with callback: `function<void(function<void(T)> callback)> provider`.
- It never provides data until requested.
- Each data request can have different callback.
- In the basic case one request assumes one response.
- The `async_stream::next` and `async_file::get_size` are the two examples of data providers.

The `l_async::slot` allows to make data providers:
1. Create instance of `l_async::slot<T>` and give it to consumers. It is a shared_ptr to the real object. It's also a `function<void(function<void(T)> callback)> provider`. It can be called by any consumer.
2. Before the instance of `l_async::slot<T>` is given to consumers, you should take and store your own "provider" part of this slot with `auto prov = get_provider()`. It is also a `shared_ptr`.
3. When you finished your provider initialization and are ready to serve the requests, call `prov.await([](bool term) {...});`, this call will store its lambda till the moment, the consumer will either call the slot for data or destroy it. If either of this event happened, this lambda will be awoken with a bool parameter:
  - `term=true` - if it is destroyed. In this case you need to destroy your `prov` object and return,
  - `term=false` - if data is requested. In this case you need to prepare data sync or async, doesn't matter, and call you'r `prov()` with your data. Yes it is also a `function(T)`

Example

Async data provider that takes two async data providers that provide streams of `optional<T>` and `optional<Y>` (where `nullopt` signals the end of stream), and returns their inner join in the form of the stream of `optional<pair<T, Y>>`
```C++
template<typename T, typename Y>
function<void(function<void(optional<pair<T, Y>>)>)> inner_join(
	function<void(function<void(optional<T>)>)> seq_a,
	function<void(function<void(optional<T>)>)> seq_b)
{
	slot<optional<pair<T, Y>>> result;  // [1]
	loop zipping([
		seq_a = move(seq_a),
		seq_b = move(seq_b),
		sink = result.get_provider()  // [2]
	](auto next) mutable {
		sink.await([&, next](bool term) {  // [3]
			if (term) return;  // [4]
			l_async::result<pair<optional<int>, optional<int>>> combined([&, next](auto combined) mutable {  // [5]
				sink(combined.first && combined.second  // [6]
					? optional(pair{move(*combined.first), move(*combined.second)})
					: nullopt);
				next();  // [7]
			});
			seq_a(combined.setter(combined->first));  // [8]
			seq_b([combined](auto value) mutable { combined->second = move(value); }); // [9] (the same as [8], but less clear)
		});
	});
	return result;  // [9]
}
```
Where
- We create \[1] and return \[9] our data provider `slot`.
- We take and hold our counterpart \[2]
- We register that we are ready to serve the next request \[3]
- When the consumer deletes the slot object (we gave it in \[9]), we detect it in \[4] and delete out context data by not calling and just releasing the `next`. This also deletes `seq_a` and `seq_b`. 
- On the actual data request from consumer we create the `combined` `result` to accumulate the results of two parallel outgoing requests, that could be received in any order and possibly asynchronously.
- Then we perform two parallel requests on `seq_a` \[8] and `seq_b` \[9]. Please note, that these two line do exactly the same job. Line \[9] is just an illustration of what `result::setter` does.
- After two results are done fetching, we notify our consumer by calling `sinc` at \[6]
- And by calling `next` \[7] we restart our `zipping` loop, which calls the `sink.await` and make us ready to receive a new request.
- Our `loop` will continue working until our consumer deletes our `slot` object and we stop at \[4].

Slots are useful for building the chained data providers and for creating state machines, because `await` in the same `slot` can be called with different lambdas.

## Structure
- `include/l_async.h` - single header library itself,
- `docs/*` - sync and async examples mentioned in this readme,
- `examples/*` - more detailed per-primitive examples,
- `tests/single_thread_executor.h` - helper class to imitate asynchronous framework for tests and examples,
- `tests/gunit.*` - lightweight testing framework, that mimics the very basic parts of GUNIT (just to avoid external depts),
- `tests/*` - other tests,
- `CMakeLists.txt` - builds test and examples.
