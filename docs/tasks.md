
A `Task` is the fundamental unit of work in the [[Runtime|runtime]].

There are 7 classes in the `task.h` file, a `Task` base class and 6 classes which derive from `Task`. They are
```
Task_ARGS_RTR_1
Task_ARGS_RTR_2
Task_ARGS_VOID_1
Task_ARGS_VOID_2
Task_1
Task_2
```

For all classes, the constructor does nothing but assign values to fields from arguments.
### Base Class
The `Task` base class is a mostly empty parent class with fields `start`, `end`, and `batch_size`. It has a single virtual function `execute`. `start` and `end` are the starting and ending indices of a given vector to be managed by the task, and the `batch_size` determines how much of the vector will be operated on at once.

### `Task_ARGS_RTR`
`Task_ARGS_RTR_1` and `Task_ARGS_RTR_2` are very similar. They differ only in the number of arguments they take (the 1 and 2 in the names). They both have the fields
```
InputType x;
ReturnType res;
std::function<> func;
```
`Task_ARGS_RTR_2` has the additional input `InputType y` (the same type as `x`). The function for `Task_ARGS_RTR_1` has signature 
`std::function<bool(InputType&, ReturnType&, const int&, const int&)>`

while the function for `Task_ARGS_RTR_2` has signature (an extra argument for `y`)
`std::function<bool(InputType&, InputType&, ReturnType&, const int&, const int&)>`

The `execute` function simply iterates over the batches and calls the function. For each batch, it calculates the indices to work over as `i` to `j` and calls either
`func(x, res, i, j)` 
or
`func(x, y, res, i, j)`

### `Task_ARGS_VOID`
`Task_ARGS_VOID_1` and `Task_ARGS_VOID_2` are identical to their `Task_ARGS_RTR` counterparts, except they don't accept return values (hence the void in the name).

### `Task_1` and `Task_2`
Neither of these classes are ever used in the system. They can be safely removed, but I'm hesitant to remove them without understanding their intended purpose.