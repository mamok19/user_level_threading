# UThreads - User-Level Threading Library

A lightweight threading library implemented in C++ with preemptive scheduling using virtual timer signals.

## Features

- Preemptive round-robin scheduling with configurable time quantums
- Thread states: RUNNING, READY, and BLOCKED
- Sleep functionality and manual thread blocking/resuming
- Thread termination and clean resource management
- Quantum usage statistics tracking

## API

### Initialization
- `int uthread_init(int quantum_usecs)` - Initialize library with quantum duration

### Thread Management
- `int uthread_spawn(thread_entry_point entry_point)` - Create new thread
- `int uthread_terminate(int tid)` - Terminate thread

### Thread Control
- `int uthread_block(int tid)` - Block thread
- `int uthread_resume(int tid)` - Resume blocked thread  
- `int uthread_sleep(int num_quantums)` - Sleep for specified quantums

### Information
- `int uthread_get_tid()` - Get current thread ID
- `int uthread_get_total_quantums()` - Get total quantums since init
- `int uthread_get_quantums(int tid)` - Get thread's quantum count

## Configuration

- `MAX_THREAD_NUM`: Maximum concurrent threads (typically 100)
- `STACK_SIZE`: Stack size per thread in bytes (typically 4096)

## Usage Example

```cpp
#include "uthreads.h"

void worker_thread() {
    for (int i = 0; i < 5; i++) {
        // Do work
        uthread_sleep(2); // Sleep for 2 quantums
    }
}

int main() {
    uthread_init(100000); // 100ms quantums
    
    int tid = uthread_spawn(worker_thread);
    
    // Main thread work
    for (int i = 0; i < 10; i++) {
        uthread_sleep(1);
    }
    
    uthread_terminate(tid);
    return 0;
}
```

## Building

```bash
make          # Compile library
make clean    # Clean build artifacts  
make tar      # Create distribution archive
```

## Implementation Notes

- Round-robin scheduling with timer-based preemption
- Uses `sigsetjmp`/`siglongjmp` for context switching
- Static stack allocation for each thread
- Signal masking prevents race conditions
- Compatible with UNIX-like systems (Linux, macOS, BSD)
- Requires C++11 or later
