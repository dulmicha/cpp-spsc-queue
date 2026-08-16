# Bounded Single-Producer Single-Consumer Queue

A C++17 thread-safe bounded SPSC queue library for messaging between exactly one producer thread and exactly one consumer thread.

## Requirements & Compatibility

- **C++ Standard**: C++17 or newer
- **Compiler**: GCC 7+ or Clang 6+ (Linux target)
- **Build System**: CMake 3.14+
- **Dependencies**: No external runtime or third-party library dependencies. GoogleTest is fetched automatically via CMake for running unit tests.

## Assumptions & Known Limitations

1. **Single Producer, Single Consumer**:
   - The queue is designed exclusively for access by **exactly one producer thread** (invoking push operations) and **exactly one consumer thread** (invoking pop operations) concurrently.
   - Using multiple producer threads or multiple consumer threads without external synchronization will result in data races and undefined behavior.
2. **Fixed Bounded Capacity**:
   - Queue capacity is determined at construction time and remains fixed for the lifetime of the object.

## How to Build

Configure and build the project using CMake:

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

## How to Run Tests

After building the project, tests can be run using `ctest` or by executing test binaries directly:

Using `ctest`:

```bash
cd build
ctest --output-on-failure
```

Or executing the test binaries directly:

```bash
./tests/spsc_queue_test
./tests/spsc_queue_stress_test
```

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
