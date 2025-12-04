/**
 * silo_runtime.h
 *
 * Per-site runtime context for Silo/Mako.
 * Allows multiple independent sites to run in a single process.
 *
 * Each SiloRuntime provides:
 * - An isolated MasstreeContext for the site's Masstree instances
 *   (independent epoch, thread registry, and B-tree state)
 * - Per-site core ID allocation (each site has its own core ID space)
 *
 * The following remain global (shared by all sites) by design:
 * - allocator: Shared memory pool using huge pages. Sharing is efficient
 *   and safe - allocator just manages memory regions, not site state.
 * - ticker: Global epoch ticker. Sites use MasstreeContext's epoch for
 *   their own Masstree instances, not the global ticker.
 * - rcu: Global RCU system. Safe to share since each threadinfo is
 *   associated with a specific MasstreeContext.
 *
 * This design provides site isolation where it matters (data structures,
 * core IDs) while efficiently sharing system resources (memory, threads).
 */
// @safe
// Uses RustyCpp smart pointers for memory safety

#ifndef SILO_RUNTIME_H
#define SILO_RUNTIME_H

#include <atomic>
#include <mutex>
#include <rusty/rusty.hpp>
#include "masstree/masstree_context.h"
#include "macros.h"  // For NMAXCORES

/**
 * SiloRuntime - Per-site runtime context
 * @safe - This class follows Rust-like ownership semantics
 *
 * Usage:
 *   // Create a runtime for each site
 *   rusty::Arc<SiloRuntime> site1 = SiloRuntime::Create();
 *   rusty::Arc<SiloRuntime> site2 = SiloRuntime::Create();
 *
 *   // In each site's threads:
 *   SiloRuntime::BindCurrentThread(site1.get());  // or site2
 *
 *   // Access the runtime
 *   SiloRuntime* runtime = SiloRuntime::Current();
 *   MasstreeContext* ctx = runtime->masstree_context();
 */
// @safe
class SiloRuntime {
public:
    // @safe
    // @lifetime: owned
    // Factory - create a new runtime for a site
    // Returns Arc for thread-safe shared ownership
    static rusty::Arc<SiloRuntime> Create();

    // @safe
    // Thread binding - associate current thread with a runtime
    // @param runtime: borrowed reference, caller retains ownership
    static void BindCurrentThread(SiloRuntime* runtime);

    // @safe
    // @lifetime: &'static
    // Get the runtime for the current thread
    // Returns borrowed pointer to thread-local runtime
    // Returns the default global runtime if none bound
    static SiloRuntime* Current();

    // @safe
    // @lifetime: &'static
    // Get the default global runtime (for backward compatibility)
    static SiloRuntime* GlobalDefault();

    // @safe
    // Accessors - const methods for reading state
    int id() const { return runtime_id_; }

    // @safe
    // @lifetime: &'self
    // Get the MasstreeContext for this runtime (mutable access)
    MasstreeContext* masstree_context() { return masstree_ctx_.get(); }

    // @safe
    // @lifetime: &'self
    // Get the MasstreeContext for this runtime (const access)
    const MasstreeContext* masstree_context() const { return masstree_ctx_.get(); }

    // =========================================================================
    // Core ID Management (per-runtime core ID space)
    // =========================================================================

    // Maximum cores per runtime
    static const unsigned NMaxCores = NMAXCORES;

    // @safe
    // Allocate a new core ID from this runtime's ID space
    // Returns the allocated core ID
    // Thread-safe: uses atomic fetch_add
    unsigned allocate_core_id();

    // @safe
    // Get the current core count for this runtime
    unsigned core_count() const {
        return core_count_.load(std::memory_order_acquire);
    }

    // @safe
    // Allocate a contiguous block of core IDs with alignment
    // Returns the starting core ID, or -1 if allocation would exceed max
    int allocate_contiguous_aligned_block(unsigned n, unsigned alignment);

    // @unsafe
    // Convenience: bind this runtime to current thread and also bind its MasstreeContext
    // @unsafe because MasstreeContext::BindCurrentThread is not annotated
    void BindToCurrentThread() {
        SiloRuntime::BindCurrentThread(this);
        MasstreeContext::BindCurrentThread(masstree_ctx_.get());
    }

    // Non-copyable, non-movable (prevent accidental ownership transfer)
    SiloRuntime(const SiloRuntime&) = delete;
    SiloRuntime& operator=(const SiloRuntime&) = delete;
    SiloRuntime(SiloRuntime&&) = delete;
    SiloRuntime& operator=(SiloRuntime&&) = delete;

    // @safe
    ~SiloRuntime() = default;

    // Constructor - prefer Create() factory for Arc-wrapped instances
    // @safe
    SiloRuntime();

private:

    // Instance ID for debugging/identification
    int runtime_id_;

    // @lifetime: owned
    // Owned MasstreeContext - each runtime has exactly one
    rusty::Box<MasstreeContext> masstree_ctx_;

    // Per-runtime core ID counter
    // Each runtime has its own core ID space starting at 0
    std::atomic<unsigned> core_count_{0};

    // Static members for global state management
    // Atomic counter for generating unique runtime IDs
    static std::atomic<int> s_next_runtime_id_;

    // @lifetime: 'static
    // Global default runtime (lazily initialized, lives for program duration)
    // Using rusty::Arc for thread-safe access
    static rusty::Arc<SiloRuntime> s_global_default_;

    // Mutex for thread-safe lazy initialization of global default
    static std::mutex s_global_mutex_;
};

// Thread-local runtime pointer
// @unsafe - Thread-local storage requires careful lifetime management
// The pointed-to SiloRuntime must outlive all threads using it
extern thread_local SiloRuntime* tl_silo_runtime;

#endif // SILO_RUNTIME_H
