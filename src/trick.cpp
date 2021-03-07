#include <bco/utils.h>
#include <bco/context.h>
#include <bco/executor.h>

namespace bco {

thread_local std::weak_ptr<detail::ContextBase> current_thread_ctx;

std::weak_ptr<detail::ContextBase> get_current_context()
{
    return current_thread_ctx;
}

void set_current_thread_context(std::weak_ptr<detail::ContextBase> ctx)
{
    current_thread_ctx = ctx;
}

}