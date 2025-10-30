
[笢恅](./readme.zh.md)

# Queue Library (Memory Pool Based)

This library provides a lightweight queue implementation designed for
embedded systems.  
It uses a pre-allocated memory pool (free-list + active-list) to avoid
dynamic allocation (`malloc/free`) during runtime.

## Features
- Fixed memory pool, no fragmentation
- FIFO queue semantics
- Supports variable-length messages (each push carries length)
- Thread-safe if protected externally (e.g. with mutex or IRQ lock)

## Initialization

```c
#include "Queue.h"

Queue_handle_t q;
Queue_conf_t conf = {
    .size = 16,       // number of nodes
    .item_size = 64   // maximum bytes per message
};

if (Queue_Init(&q, &conf) != QUEUE_OK) {
    // handle error
}
```

## API Reference

### `Queue_Init`

```c
Queue_Sta_t Queue_Init(Queue_handle_t *handle, const Queue_conf_t *conf);
```

* Initializes a queue with given configuration.
* Parameters:

  * `handle`: pointer to queue handle (output)
  * `conf`: pointer to configuration (node count + max item size)
* Returns:

  * `QUEUE_OK`    每 success
  * `QUEUE_ERR`   每 invalid argument or allocation failed

### `Queue_Push`

```c
Queue_Sta_t Queue_Push(Queue_handle_t handle, const void *data, uint16_t len);
```

* Pushes a message into the queue.
* Parameters:

  * `handle`: queue handle
  * `data`: pointer to message buffer
  * `len`: number of bytes to copy (≒ max item size)
* Returns:

  * `QUEUE_OK`    每 success
  * `QUEUE_FULL`  每 no free node available
  * `QUEUE_ERR`   每 invalid input

### `Queue_Pop`

```c
Queue_Sta_t Queue_Pop(Queue_handle_t handle, void *out_buf, uint16_t *out_len);
```

* Pops one message from the queue (FIFO).
* Parameters:

  * `handle`: queue handle
  * `out_buf`: buffer to copy message into
  * `out_len`: pointer to store actual message length
* Returns:

  * `QUEUE_OK`     每 success
  * `QUEUE_EMPTY`  每 queue has no message
  * `QUEUE_ERR`    每 invalid input

### `Queue_Count`

```c
size_t Queue_Count(Queue_handle_t handle);
```

* Gets the number of active messages currently in the queue.

### `Queue_FreeCount`

```c
size_t Queue_FreeCount(Queue_handle_t handle);
```

* Gets the number of free nodes available.

### `Queue_Deinit`

```c
void Queue_Deinit(Queue_handle_t *handle);
```

* Frees the memory pool and invalidates the handle.

## Example

```c
uint8_t msg[] = "Hello";
Queue_Push(q, msg, sizeof(msg));

uint8_t buf[64];
uint16_t len;
if (Queue_Pop(q, buf, &len) == QUEUE_OK) {
    // buf contains "Hello", len = 6
}
```

## Return Codes

| Code          | Meaning                               |
| ------------- | ------------------------------------- |
| `QUEUE_OK`    | Operation successful                  |
| `QUEUE_ERR`   | Invalid input or general failure      |
| `QUEUE_FULL`  | Queue is full, no free node available |
| `QUEUE_EMPTY` | Queue is empty, nothing to pop        |

```
