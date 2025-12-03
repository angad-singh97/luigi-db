/**
 * silo_runtime.h
 *
 * Per-site runtime context for Silo/Mako.
 * Allows multiple independent sites to run in a single process.
 *
 * Each SiloRuntime provides:
 * - An isolated MasstreeContext for the site's Masstree instances
 * - Site-specific RCU state tracking
 *
 * The following remain global (shared by all sites):
 * - allocator (memory pool)
 * - ticker (epoch system)
 * - rcu singleton (memory reclamation)
 * - coreid (thread-to-core mapping)
 *
 * This design allows efficient memory and resource sharing while
 * maintaining logical isolation between sites.
 */
// @safe
// Uses RustyCpp smart pointers for memory safety

#ifndef SILO_RUNTIME_H
#define SILO_RUNTIME_H

#include <atomic>
#include <mutex>
#include <rusty/rusty.hpp>
#include "masstree/masstree_context.h"

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
