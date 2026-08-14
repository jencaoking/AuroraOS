# How to apply this JARVIS Phase-1 skeleton onto AuroraOS

## Option A — copy directory

```bash
cd /path/to/AuroraOS
cp -a /path/to/aurora_jarvis_skel/ai/jarvis ai/
# optional host test
mkdir -p tests/unit
cp aurora_jarvis_skel/tests/unit/test_jarvis_core.cpp tests/unit/
```

Keep the original `ai/intent_engine.hpp` until you migrate call sites.
Use `ai/jarvis/compat_intent_engine.hpp` as a bridge if needed.

## Option B — from the tarball

```bash
tar xzf aurora_jarvis_phase1_skel.tar.gz
cp -a aurora_jarvis_skel/ai/jarvis /path/to/AuroraOS/ai/
```

## Verify on host

```bash
g++ -std=c++17 -Wall -Wextra -I. -o /tmp/test_jarvis \
    tests/unit/test_jarvis_core.cpp   # after copy, adjust -I
/tmp/test_jarvis
```

## CMake (minimal, optional)

In the firmware `CMakeLists.txt` or a new `ai/CMakeLists.txt`:

```cmake
# Header-only for Phase 1 — just expose include path
target_include_directories(auroraOS PUBLIC ${CMAKE_SOURCE_DIR}/ai)
# Later: add jarvis_service.cpp when you move logic out of headers
```

No source files need to be compiled yet; everything is header-only.

## Next integration steps

1. Create a dedicated JARVIS task in `apps/` or `services/`.
2. Register ActionHooks to real AppControlBlock / UART / notification paths.
3. Feed real sensor data from SensorManager into `JarvisCore::feed_*`.
4. Add Kconfig `CONFIG_AI_JARVIS`.
5. (Later) API / SoftBus endpoints that call `feed_text` / `inject_intent`.
