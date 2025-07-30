import os
import re
import pandas as pd
import matplotlib.pyplot as plt

# Path to data directory
# data_dir = './1014-1457/raw_data'
data_dir = '../../../results/communicator-benchmark/thread-profiling/1019-0556/raw_data'
parent_dir = os.path.dirname(data_dir)

e2e_pattern = r"== e2e time: (\d+\.\d+) ms"
worker_total_time_avg_pattern = r"total\s+(\d+\.\d+)"
worker_comm_time_avg_pattern = r"total_comm\s+(\d+\.\d+)"
worker_run_time_avg_pattern = r"run\s+(\d+\.\d+)"
main_thread_time_pattern = r"== Main Thread other: (\d+\.\d+)"


def parse_files():

    environment = "lan"
    protocol = "3pc"
    communicator_list = ["MPI", "NOCOPY"]
    op_list = ["AND", "EQ", "GR", "RCA"]
    # thread_list = [1, 2, 4, 8, 16, 32, 64]
    thread_list = [1, 2, 4, 8]


    data_list = []

    for comm in communicator_list:
        for op in op_list:
            for thread in thread_list:
                file_name = f"{comm}-{environment}-{protocol}-{op}-{thread}thr.txt"
                file_path = os.path.join(data_dir, file_name)

                e2e_time = None
                worker_total_time_avg = None
                worker_comm_time_avg = None
                worker_run_time_avg = None
                main_thread_time = None

                with open(file_path, 'r') as file:
                    lines = file.readlines()

                for line in lines:
                    match = re.search(e2e_pattern, line)
                    if match:
                        e2e_time = float(match.group(1))
                    
                    match = re.search(worker_total_time_avg_pattern, line)
                    if match:
                        worker_total_time_avg = float(match.group(1))

                    match = re.search(worker_comm_time_avg_pattern, line)
                    if match:
                        worker_comm_time_avg = float(match.group(1))
                    
                    match = re.search(worker_run_time_avg_pattern, line)
                    if match:
                        worker_run_time_avg = float(match.group(1))
                    
                    match = re.search(main_thread_time_pattern, line)
                    if match:
                        main_thread_time = float(match.group(1))

                        data_list.append([
                            comm, environment, protocol, op, thread, e2e_time,
                            worker_total_time_avg, worker_comm_time_avg,
                            worker_run_time_avg, main_thread_time])              


    df = pd.DataFrame(data_list, columns=[
        'communicator', 'environment', 'protocol', 'operation', 'threads',
        'e2e_time', 'worker_total_time_avg', 'worker_comm_time_avg',
        'worker_run_time_avg', 'main_thread_time'])

    return df


def plot_overall_grid(df, communicator, file_name):
    ops = ['AND', 'EQ', 'GR', 'RCA']
    fig, axs = plt.subplots(2, 2, figsize=(14, 10))
    
    comm_display = communicator
    
    for i, op in enumerate(ops):
        df_op = df[(df['communicator'] == communicator) & (df['operation'] == op)]
        
        protocol = df_op['protocol'].iloc[0].upper()
        
        ideal_times = [df_op['worker_total_time_avg'].iloc[0] / threads for threads in df_op['threads']]
        
        ax = axs[i // 2, i % 2]
        
        ax.plot(df_op['threads'], df_op['e2e_time'], label='E2E', marker='^')
        ax.plot(df_op['threads'], df_op['worker_total_time_avg'], label='Worker (Total)', marker='^')
        ax.plot(df_op['threads'], ideal_times, label='Worker (Ideal)', marker='+', linestyle='--')
        ax.plot(df_op['threads'], df_op['worker_comm_time_avg'], label='Worker (Communication)', marker='o')
        ax.plot(df_op['threads'], df_op['worker_run_time_avg'], label='Worker (Computation)', marker='o')
        
        ax.set_title(f'Primitive: {op}')
        ax.set_xlabel('Threads')
        ax.set_ylabel('Time (ms)')
        ax.set_ylim(0)
        ax.legend(loc='upper right')

    fig.suptitle(f'Primitives: {comm_display} [{protocol}]', fontsize=16, y=0.98)
    
    plt.tight_layout(rect=[0, 0, 1, 0.98])
    
    plt.savefig(os.path.join(parent_dir, file_name))
    plt.close()


def plot_breakdown_grid(df, communicator, file_name, data_type="Computation"):
    ops = ['AND', 'EQ', 'GR', 'RCA']
    fig, axs = plt.subplots(2, 2, figsize=(14, 10))

    if data_type == "Computation":
        data_column = 'worker_run_time_avg'
        data_label = 'Worker Computation'
    elif data_type == "Communication":
        data_column = 'worker_comm_time_avg'
        data_label = 'Worker Communication'
    else:
        raise ValueError("Invalid data type")
    
    comm_display = communicator
    
    for i, op in enumerate(ops):
        df_op = df[(df['communicator'] == communicator) & (df['operation'] == op)]
        
        protocol = df_op['protocol'].iloc[0].upper()
        
        ideal_times = [df_op[data_column].iloc[0] / threads for threads in df_op['threads']]
        
        ax = axs[i // 2, i % 2]
        
        ax.plot(df_op['threads'], df_op[data_column], label=data_label, marker='o')
        ax.plot(df_op['threads'], ideal_times, label=f'{data_label} (Ideal)', marker='+', linestyle='--')
        
        ax.set_title(f'Primitive: {op}')
        ax.set_xlabel('Threads')
        ax.set_ylabel('Time (ms)')
        ax.set_ylim(0)
        ax.legend(loc='upper right')

    fig.suptitle(f'Primitives: {comm_display} {data_type} Scaling [{protocol}]', fontsize=16, y=0.98)
    
    plt.tight_layout(rect=[0, 0, 1, 0.98])
    
    plt.savefig(os.path.join(parent_dir, file_name))
    plt.close()


def plot_communicator_comparison(df, file_name, data_type="Computation"):
    ops = ['AND', 'EQ', 'GR', 'RCA']

    fig, axs = plt.subplots(2, 2, figsize=(14, 10))

    if data_type == "Computation":
        data_column = 'worker_run_time_avg'
        data_label = 'Worker Computation'
    elif data_type == "Communication":
        data_column = 'worker_comm_time_avg'
        data_label = 'Worker Communication'
    else:
        raise ValueError("Invalid data type")
    
    for i, op in enumerate(ops):        
        df_mpi = df[(df['communicator'] == "MPI") & (df['operation'] == op)]
        df_nocopy = df[(df['communicator'] == "NOCOPY") & (df['operation'] == op)]

        protocol = df_mpi['protocol'].iloc[0].upper()

        ax = axs[i // 2, i % 2]

        ax.plot(df_mpi['threads'], df_mpi[data_column], label=f"MPI ({data_label})", marker='o')
        ax.plot(df_nocopy['threads'], df_nocopy[data_column], label=f"SocketComm2 ({data_label})", marker='o')

        ax.set_title(f'Primitive: {op}')
        ax.set_xlabel('Threads')
        ax.set_ylabel('Time (ms)')
        ax.set_ylim(0)
        ax.legend(loc='upper right')

    fig.suptitle(f'Primitives: {data_type} Scaling Comparison [{protocol}]', fontsize=16, y=0.98)
    
    plt.tight_layout(rect=[0, 0, 1, 0.98])
    
    plt.savefig(os.path.join(parent_dir, file_name))
    plt.close()


df = parse_files()

df.to_csv(os.path.join(parent_dir, 'thread-profiling.csv'), index=False)

plot_overall_grid(df, 'MPI', 'mpi-overall.png')
plot_overall_grid(df, 'NOCOPY', 'nocopy-overall.png')

plot_breakdown_grid(df, 'MPI', 'mpi-computation.png', "Computation")
plot_breakdown_grid(df, 'NOCOPY', 'nocopy-computation.png', "Computation")

plot_breakdown_grid(df, 'MPI', 'mpi-communication.png', "Communication")
plot_breakdown_grid(df, 'NOCOPY', 'nocopy-communication.png', "Communication")

plot_communicator_comparison(df, 'comparison-computation.png', "Computation")
plot_communicator_comparison(df, 'comparison-communication.png', "Communication")
