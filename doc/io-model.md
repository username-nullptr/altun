# Execution and I/O model

[Documentation](README.md)

altun asynchronous APIs follow Asio and Riwo conventions. Executor-bound types expose `executor_type`, `executor_t`, and `get_executor()`; convenience aliases use the default Riwo execution context.

The Asio provider is selected by Riwo. `RIWO_ASIO_PROVIDER=BOOST` builds altun
against Boost.Asio; `BUNDLED` and `EXTERNAL` select standalone Asio. Public
error-code arguments and tokens on the I²C, SPI, GPIO-line, udev, and serial
interfaces accept either `std::error_code` or the provider type, while
asynchronous completion signatures use `riwo::error_code`.

## Completion and lifetime

- Completion handlers are submitted through the associated executor, including immediate failures.
- Associated executors, immediate executors, allocators, and cancellation slots propagate from the completion token.
- Changing a handler executor changes where completion runs; it does not make the I/O object thread-safe.
- Caller-owned buffers and other borrowed arguments must remain valid until completion. Cancellation is complete only when its completion path finishes.
- Fixed-size I²C/SPI reads that return a `Buffer` own their result storage. Detached I²C/SPI/serial writes copy outgoing data; ordinary asynchronous writes borrow it.

## Concurrency

Distinct objects may be used concurrently. Access to one object—including open, close, initiation, completion, cancellation, and destruction—must be serialized unless that type documents otherwise.

When several threads run one context, bind the object and all access to the same strand:

```cpp
asio::io_context context;
auto strand = asio::make_strand(context);
altun::bus::basic_i2c<decltype(strand)> sensor(strand);

asio::co_spawn(strand, [&]() -> riwo::awaitable<void> {
    std::array<std::uint8_t, 2> value {};
    co_await sensor.read(0x00, riwo::buffer(value), riwo::use_awaitable);
}, riwo::detached);
```

A mutex around only the initiating call does not serialize later completion or cancellation.

## Interface-specific rules

| Interface | Rule |
| --- | --- |
| I²C/SPI | Do not overlap operations on the same device object. |
| GPIO | `wait_event()` is one-shot; `on_event()` is persistent. Serialize lifecycle changes with event handling. |
| udev events | Signal slots run from the monitor execution path; slow synchronous slots apply backpressure. |
| Serial binding | Keep one active sequence per read/write direction and serialize rule lifecycle changes. |
