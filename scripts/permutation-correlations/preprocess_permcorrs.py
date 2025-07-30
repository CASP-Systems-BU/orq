import os
import subprocess
from concurrent.futures import ThreadPoolExecutor, as_completed
import re
import time
import sys

# run the task on a given core
def run_task_on_core(task_id, core_id, executable):
    command = f"taskset -c {core_id} {executable} {task_id}"
    result = subprocess.run(command, shell=True, capture_output=True, text=True)
    time = re.search(r'(\d+ms)', result.stdout)
    time_s = float(time.group(1)[:-2]) / 1000
    return f"Task {task_id} finished on core {core_id}: {time_s}s"

# run tasks and assign cores
def run_tasks(executable, num_tasks, num_cores, verbose):
    with ThreadPoolExecutor(max_workers=num_cores) as executor:
        # Submit the initial batch of tasks, one per core.
        future_to_core = {
            executor.submit(run_task_on_core, task_id, task_id % num_cores, executable): task_id
            for task_id in range(min(num_tasks, num_cores))
        }
        
        # As each task completes, assign the next one to an available core.
        next_task_id = num_cores
        while future_to_core:
            # Wait for any core to finish.
            for future in as_completed(future_to_core):
                task_id = future_to_core.pop(future)
                try:
                    if verbose:
                        print(future.result())
                except Exception as exc:
                    print(f'Task {task_id} generated an exception: {exc}')
                
                # If there are more tasks, assign the next one to this core.
                if next_task_id < num_tasks:
                    future_to_core[executor.submit(run_task_on_core, next_task_id, next_task_id % num_cores, executable)] = next_task_id
                    next_task_id += 1

if __name__ == "__main__":
    size = int(sys.argv[1])
    num_tasks = int(sys.argv[2])
    num_cores = int(sys.argv[3])
    bitwidth = 32
    if (len(sys.argv) > 4):
        bitwidth = int(sys.argv[4])
    verbose = True if len(sys.argv) > 4 and sys.argv[4] == '-v' else False
    executable = f'./secJoinfrontend -bench -AltModPerm -n {size} -m {bitwidth} &'

    start_time = time.time()

    run_tasks(executable, num_tasks, num_cores, verbose)
    
    end_time = time.time()
    total_time = end_time - start_time
    print(f"Total execution time: {total_time:.2f} seconds")

