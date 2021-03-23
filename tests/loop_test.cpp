#include <functional>
using std::function;

#include <optional>
using std::optional;
using std::nullopt;

#include "single_thread_executor.h"
#include "gunit.h"
#include "l_async.h"

namespace
{
    class async_or_async_data_stream
    {
        int i = 0;
        testing::single_thread_executor& ex;

    public:
        async_or_async_data_stream(testing::single_thread_executor& ex)
            : ex(ex)
        {}

        void get_next(function<void(optional<int>)> callback)
        {
            if (i < 5) {
                callback(optional<int>(++i));
            } else {
                ex.schedule([callback = move(callback), v = ++i]{
                    callback(v < 10 ? optional<int>(v) : nullopt);
                });
            }
        }
    };

    struct no_copy_data
    {
        no_copy_data() noexcept
        {}

        no_copy_data(no_copy_data&&) noexcept
        {}

        no_copy_data(const no_copy_data&)
        {
            ASSERT_TRUE(false) << "this element must not be copied";
        }
    };

    TEST(LAsync, BasicTest)
    {
        testing::single_thread_executor executor;
        async_or_async_data_stream stream(executor);
        l_async::loop test([=, expected = 0, depth = 0, d = no_copy_data()](auto next) mutable {
            depth++;
            ASSERT_LT(depth, 2) << "loop recursions must be prevented";
            stream.get_next([&, next](auto data) {
                if (data) {
                    ASSERT_EQ(*data, ++expected) << "mismatched data";
                    next();
                } else {
                    ASSERT_EQ(expected, 9) << "incomplete data";
                }
            });
            depth--;
        });
        executor.execute();
    }
}
