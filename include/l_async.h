#ifndef _L_ASYNC_H_
#define _L_ASYNC_H_

#include <memory>
#include <functional>
#include <cassert>

namespace l_async
{
	class loop
	{
		std::shared_ptr<
			std::pair<
			std::function<void(const loop&)>,
			bool>> body;

	public:
		loop(std::function<void(std::function<void()>)> body)
		{
			this->body = std::make_shared<
				std::pair<
					std::function<void(const loop&)>,
					bool>>(std::move(body), false);
			operator()();
		}

		void operator() ()
		{
			while ((body->second = !body->second))
			{
				body->first(*this);
			}
		}
	};

	template<typename T>
	class result
	{
		struct data_t
		{
			T data;
			std::function<void(T)> callback;

			data_t(T data, std::function<void(T)> callback)
				: data(std::move(data))
				, callback(std::move(callback))
			{}

			~data_t()
			{
				callback(std::move(data));
			}
		};

		std::shared_ptr<data_t> ptr;

	public:
		result(std::function<void(T)> callback, T initial_value = T())
			: ptr(std::make_shared<data_t>(
				std::move(initial_value),
				std::move(callback)))
		{}

		T& data()
		{
			return ptr->data;
		}
	};

	template<typename T>
	class unique
	{
		T data;

	public:
		unique(T&& data)
			: data(std::move(data))
		{}

		unique(unique<T>&& src)
			: data(std::move(src.data))
		{}

		unique(const unique<T>& data)
		{
			assert(false);
		}

		operator T& ()
		{
			return data;
		}

		operator const T& () const
		{
			return data;
		}

		T& operator-> ()
		{
			return data;
		}

		void operator= (const unique<T>&) = delete;
	};
}

#endif  // _L_ASYNC_H_
