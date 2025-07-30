import re
import os
import pandas as pd
import glob
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

# Path to data directory
data_dir = './1014-2347/raw_data'
# data_dir = '../../../results/communicator-benchmark/thread-latency/1015-0003/raw_data'
parent_dir = os.path.dirname(data_dir)

experiment_pattern = r"====\s+(\d+)\s+rows;\s+(\d+)\s+threads\s+===="
header_pattern = r"Vector:\s*(\d+)\s*x\s*(\d+)b\s*\|\s*Sample count:\s*(\d+)\s*\|\s*Communicator:\s*(.+)"


def parse_files():
    data_list = []
    for file_name in os.listdir(data_dir):

        name = file_name.split('.')[0]
        environment = name.split('-')[0]

        print(f"Processing {name}")

        file_path = os.path.join(data_dir, file_name)
        with open(file_path, 'r') as file:
            lines = file.readlines()
        
        for i, line in enumerate(lines):
            match = re.match(experiment_pattern, line)
            if match:
                threads = match.group(2)

            match = re.match(header_pattern, line)
            if match:
                vector_size = int(match.group(1))
                bitwidth = match.group(2)
                sample_count = match.group(3)
                communicator = match.group(4)
                if i + 1 >= len(lines):
                    raise ValueError("Missing data")

                latency_samples = (lines[i + 1].strip()).split(',')
                latency_samples = [float(latency_samples) for latency_samples in latency_samples]
                if len(latency_samples) != int(sample_count):
                    raise ValueError("Missing samples")

                data_list.append([communicator, environment, threads, vector_size, bitwidth, sample_count, latency_samples])

    df = pd.DataFrame(data_list, columns=['communicator', 'environment', 'threads', 'vector_size', 'bitwidth', 'sample_count', 'latency_samples'])

    df['average_latency'] = df['latency_samples'].apply(lambda x: sum(x) / len(x))
    df['median_latency'] = df['latency_samples'].apply(lambda x: sorted(x)[len(x) // 2])
    df['std_dev'] = df['latency_samples'].apply(lambda x: np.std(x))

    df['latency_per_element_ns'] = (df['average_latency'] / df['vector_size']) * 1000

    return df


def generate_average_plot(df, file_name):
    communicators = sorted(df['communicator'].unique())

    plt.figure(figsize=(10, 6))

    for communicator in communicators:
        comm_data = df[df['communicator'] == communicator]
        
        plt.plot(comm_data['threads'], comm_data['average_latency'], 
                 label=f'{communicator}', marker='o')
        # plt.errorbar(comm_data['threads'], comm_data['average_latency'], 
        #              yerr=comm_data['std_dev'], label=f'{communicator}', marker='o', capsize=5)

    plt.title('ExchangeShares: Average Latency per Thread per Batch')
    plt.xlabel('Threads')
    plt.ylabel('Average Latency (µs)')

    plt.ylim(0)
    plt.tight_layout()
    
    # plt.xscale("log", base=2)
    plt.xticks(df["threads"], labels=df["threads"])

    plt.legend(loc='upper left')

    plt.savefig(os.path.join(parent_dir, file_name))
    plt.close()


if __name__ == "__main__":

    df = parse_files()

    # Generate plots
    generate_average_plot(df, 'average_latency.png')

    df.drop(columns=['latency_samples']).to_csv(os.path.join(parent_dir, 'thread-latency.csv'), index=False)
