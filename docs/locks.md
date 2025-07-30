
In the runtime, each thread is given three locks.
- `task_set_mutex`
- `task_execute_mutex`
- `task_finished_mutex`

Locks are manipulated from the parallel execution functions and from the main loop. Below we describe how each lock is managed.

### Task Set Mutex
The task set mutex indicates whether the thread has been assigned a task. When we enter the thread-specific part of a parallel execution function, we lock the task set mutex. It is unlocked after the task execution function returns in the main loop (and only here). Its default state is to be unlocked, and it is locked when a task is ready to be assigned to the thread.

### Task Execute Mutex
The task execute mutex indicates whether a task is ready to be executed. Its default state is to be locked by the main thread, indicating that the worker thread cannot yet execute a task. Inside a parallel execution function, once a task is assigned to a thread, the main thread unlocks the lock. The thread's main loop can then successfully acquire the lock and execute its task. **This might be undefined behavior.**

