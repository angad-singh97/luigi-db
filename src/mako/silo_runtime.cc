/**
 * silo_runtime.cc
 *
 * Implementation of per-site SiloRuntime.
 * @safe - Uses RustyCpp smart pointers for memory safety
 */

#include "silo_runtime.h"
#include "amd64.h"  // For nop_pause()
#include "util.h"   // For slow_round_up()

// Thread-local runtime pointer
// @unsafe - Thread-local storage with raw pointer for performance
// Safety invariant: pointed-to SiloRuntime must outlive the thread
thread_local SiloRuntime* tl_silo_runtime = nullptr;

// Static member initialization
std::atomic<int> SiloRuntime::s_next_runtime_id_{0};
rusty::Arc<SiloRuntime> SiloRuntime::s_global_default_{nullptr};
std::mutex SiloRuntime::s_global_mutex_;

// @safe
SiloRuntime::SiloRuntime()
    : runtime_id_(s_next_runtime_id_.fetch_add(1, std::memory_order_relaxed)),
      // @unsafe {
      // MasstreeContext is non-copyable/non-movable, so use raw pointer constructor
      masstree_ctx_(new MasstreeContext()) {
      // }
    // MasstreeContext is created inline and owned by this SiloRuntime
}

// @safe
// @lifetime: owned
rusty::Arc<SiloRuntime> SiloRuntime::Create() {
    // Create new SiloRuntime wrapped in Arc for thread-safe sharing
    return rusty::Arc<SiloRuntime>::make();
}

// @safe
void SiloRuntime::BindCurrentThread(SiloRuntime* runtime) {
    // @unsafe {
    // Store raw pointer in thread-local storage
    // Safety: Caller must ensure runtime outlives all threads that use it
    tl_silo_runtime = runtime;
    // }

    // Also bind the MasstreeContext for this runtime
    if (runtime) {
        MasstreeContext::BindCurrentThread(runtime->masstree_ctx_.get());
    }
}

// @safe
// @lifetime: &'static
SiloRuntime* SiloRuntime::Current() {
    if (tl_silo_runtime) {
        return tl_silo_runtime;
    }
    // Return global default for backward compatibility
    return GlobalDefault();
}

// @safe
// @lifetime: &'static
SiloRuntime* SiloRuntime::GlobalDefault() {
    // Fast path: already initialized
    if (s_global_default_) {
        return s_global_default_.get_mut();
    }

    // Slow path: thread-safe lazy initialization
    std::lock_guard<std::mutex> lock(s_global_mutex_);

    // Double-check after acquiring lock
    if (!s_global_default_) {
        s_global_default_ = Create();
    }

    return s_global_default_.get_mut();
}

// =========================================================================
// Core ID Management
// =========================================================================

// @safe
unsigned SiloRuntime::allocate_core_id() {
    unsigned id = core_count_.fetch_add(1, std::memory_order_acq_rel);
    ALWAYS_ASSERT(id < NMaxCores);
    return id;
}

// @safe
int SiloRuntime::allocate_contiguous_aligned_block(unsigned n, unsigned alignment) {
    using namespace util;
retry:
    unsigned current = core_count_.load(std::memory_order_acquire);
    const unsigned rounded = slow_round_up(current, alignment);
    const unsigned replace = rounded + n;
    if (unlikely(replace > NMaxCores))
        return -1;
    if (!core_count_.compare_exchange_strong(current, replace, std::memory_order_acq_rel)) {
        nop_pause();
        goto retry;
    }
    return rounded;
}
