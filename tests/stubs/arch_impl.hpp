#ifndef ARCH_IMPL_HPP
#define ARCH_IMPL_HPP

// =============================================================================
// arch_impl.hpp — Host-native STUB
//
// The real arch/arm/cortex-m/cm4/arch_impl.hpp provides Cortex-M4 inline
// assembly bodies for the Arch:: functions declared in arch_api.hpp.
// kernel/arch_api.hpp ends with:  #include "arch_impl.hpp"
//
// Important: The declarations in arch_api.hpp have no 'noexcept'.
// These inline definitions must match exactly or the compiler will error
// "different exception specifier".  So we intentionally omit noexcept here.
// =============================================================================

#include <cstdint>
#include <stdexcept>

namespace Arch {

// Match declarations in arch_api.hpp exactly (no noexcept, same signature).
inline void disable_interrupts()           {}
inline void enable_interrupts()            {}
inline uint32_t irq_save()                 { return 0u; }
inline void irq_restore(uint32_t /*flags*/) {}
inline void wait_for_interrupt()           {}
inline void systick_init(uint32_t /*hz*/)  {}
inline void trigger_context_switch()       {}

inline uint32_t* init_thread_stack(void (*/*entry*/)(void),
                                   uint32_t* stack_space,
                                   uint32_t  stack_size) {
    // Return the logical stack top (high address; stack grows downward).
    return stack_space + stack_size / sizeof(uint32_t);
}

[[noreturn]] inline void start_first_task(uint32_t* /*stack_ptr*/,
                                          void (*/*entry*/)(void)) {
    throw std::logic_error(
        "Arch::start_first_task must not be called from host unit tests");
}

}  // namespace Arch

#endif  // ARCH_IMPL_HPP
