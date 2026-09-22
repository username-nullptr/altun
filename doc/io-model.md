# Execution and I/O model

[Documentation home](README.md) · [Building and integration](getting-started.md)

The asynchronous libEMpp interfaces follow the Asio and LibGS execution model. Public types bound to an executor provide `executor_type`, `executor_t`, and `get_executor()`. Convenience types that are not explicitly bound to an executor use the default LibGS `io_context`.

## Completion handlers

- An asynchronous operation never invokes its completion handler directly on the initiating function's call stack. Even immediate parameter, state, or device-validation failures are submitted through the associated immediate executor.
- The completion token's associated executor, immediate executor, allocator, and cancellation slot propagate down the operation. If the token does not specify an executor, the I/O object's `get_executor()` is the fallback.
- A custom operation that does not complete immediately keeps both the I/O executor and completion executor alive until completion, then submits the completion handler to the token's associated executor.
- Binding a different executor to a handler changes only where the completion handler runs. It does not make the same I/O object thread-safe.
- Unless an overload explicitly says that it copies or retains data, borrowed arguments such as buffers must remain valid until the operation completes. Cancellation is asynchronous; referenced data cannot be released until cancellation completion is received.

## Thread safety and strands

Unless a type says otherwise, the standard Asio contract applies: distinct objects may be used concurrently, but sharing one object is unsafe. Calls to `open`, `close`, synchronous operations, asynchronous initiation, and related handlers on the same object must be serialized. Destruction must not race with unfinished access to the object.

When multiple threads run one `io_context`, bind the object and every coroutine or handler that accesses it to the same strand:

```cpp
asio::io_context context;
auto strand = asio::make_strand(context);
libempp::bus::basic_i2c<decltype(strand)> sensor(strand);

asio::co_spawn(strand, [&]() -> libgs::awaitable<void>
{
    std::array<std::uint8_t, 2> value {};
    co_await sensor.read(0x00, libgs::buffer(value), libgs::use_awaitable);
}, libgs::detached);
```

Wrapping only the initiating call in a mutex is not enough to serialize the entire asynchronous lifecycle. Completion handlers, cancellation, and closing must follow the same serialization rule.

## Module rules

| Module or type | Rules for operations on the same object |
| --- | --- |
| I²C / SPI | Synchronous calls execute device transactions directly. Asynchronous transactions start on the object's executor and complete on the token's associated executor. Do not overlap access to the same device object. |
| GPIO | Each `wait_event()` is a one-shot wait and supports cancellation. Persistent `on_event()` callbacks are submitted serially through the object's internal strand. The caller must still serialize lifecycle operations. |
| udev event | Monitor reads and signal dispatch use the object's executor. Synchronous slots apply backpressure to the event loop. |
| Serial-port binding | Only one active read or write sequence should exist per serial-port direction. Rules, hot-plug state, and lifecycle operations must be accessed serially. |

Normal asynchronous writes for I²C, SPI, and serial ports borrow the caller's buffer. Detached writes copy the transmitted data. Fixed-length I²C/SPI `read<Buffer>()` operations retain their own result storage. See each module guide for exact differences.
