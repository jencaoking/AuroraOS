/**
 * @file compat_intent_engine.hpp
 * @brief Compatibility bridge from the original IntentEngine API to JarvisCore.
 *
 * Original call site:
 *   IntentEngine::process_sensors(fitness_app, ctx);
 *
 * This header provides a real working bridge. App transitions still go through
 * ActionHooks registered on JarvisCore (platform must set them once at init).
 */
#ifndef AURORA_JARVIS_COMPAT_INTENT_ENGINE_HPP
#define AURORA_JARVIS_COMPAT_INTENT_ENGINE_HPP

#include "jarvis_core.hpp"

struct AppControlBlock;

namespace aurora {
namespace jarvis {

inline void process_sensors_compat(uint32_t steps, uint32_t now_ms) {
    auto& j = JarvisCore::instance();
    if (!j.ready()) {
        j.init();
    }
    j.feed_steps(steps, now_ms);
    j.tick(now_ms);
    j.process_events();
}

}  // namespace jarvis
}  // namespace aurora

#ifndef AURORA_DISABLE_LEGACY_INTENT
class IntentEngine {
public:
    struct Context {
        uint32_t last_steps   = 0;
        uint32_t idle_counts  = 0;
        uint32_t now_ms       = 0;
    };

    static void process_sensors(AppControlBlock& /*fitness_app*/, Context& ctx) {
        static uint32_t fallback_ms = 0;
        const uint32_t now = ctx.now_ms ? ctx.now_ms : (fallback_ms += 1000);

        aurora::jarvis::process_sensors_compat(ctx.last_steps, now);

        const auto& uc = aurora::jarvis::JarvisCore::instance().context();
        ctx.idle_counts = uc.idle_seconds;
    }
};
#endif

#endif  // AURORA_JARVIS_COMPAT_INTENT_ENGINE_HPP
