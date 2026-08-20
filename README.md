# Bounded Single-Producer Single-Consumer Queue

A C++17 thread-safe bounded SPSC queue library for messaging between exactly one producer thread and exactly one consumer thread.

## Requirements & Compatibility

- **C++ Standard**: C++17 or newer
- **Compiler**: GCC 7+ or Clang 6+ (Linux target)
- **Build System**: CMake 3.14+
- **Dependencies**: No external runtime or third-party library dependencies. GoogleTest is fetched automatically via CMake for running unit tests.

## Assumptions & Known Limitations

1. **Single Producer, Single Consumer Only**:
   The queue is designed for exactly one producer thread (calling `try_push` / `emplace`)
   and exactly one consumer thread (calling `try_pop`) operating concurrently.  
   Using multiple producers or multiple consumers without external synchronization
   results in undefined behavior.

2. **Fixed Bounded Capacity (Power of Two)**:
   Capacity is set at construction time and rounded up to the next power of two
   to enable fast bitwise index wrapping (`index & mask`) instead of modulo division.
   For example, requesting capacity 10 allocates 16 slots.

3. **Lock-Free, Not Wait-Free**:
   `try_push` and `try_pop` are non-blocking (they return `false` immediately if
   the queue is full/empty). Callers that need blocking semantics must spin or
   implement their own back-off strategy.

4. **Move-Only Type Support**:
   The queue stores elements in uninitialized memory (placement `new` /
   `std::destroy_at`), so types without default constructors (e.g.
   `std::unique_ptr`) work correctly. `T` must be move-constructible and
   move-assignable.

5. **Observation Functions Are Approximate**:
   `empty()`, `full()`, and `size()` return a snapshot that may be stale under
   concurrent access. They are exact only when the queue is quiescent.

## How to Build

Configure and build the project using CMake:

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

## How to Run Tests

After building the project, tests can be run using `ctest` or by executing test binaries directly:

Using `ctest`:

```bash
ctest --test-dir build --output-on-failure
```

Or run test binaries directly:

```bash
./tests/spsc_queue_test
./tests/spsc_queue_stress_test
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
