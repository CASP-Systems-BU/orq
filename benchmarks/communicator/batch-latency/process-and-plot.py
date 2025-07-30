import re
import os
import pandas as pd
import glob
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.patches import Patch

# Path to data directory
data_dir = './1014-1457/raw_data'
# data_dir = '../../../results/communicator-benchmark/batch-latency/1015-0003/raw_data'
parent_dir = os.path.dirname(data_dir)

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

                data_list.append([communicator, environment, vector_size, bitwidth, sample_count, latency_samples])

    df = pd.DataFrame(data_list, columns=['communicator', 'environment', 'vector_size', 'bitwidth', 'sample_count', 'latency_samples'])

    df['average_latency'] = df['latency_samples'].apply(lambda x: sum(x) / len(x))
    df['median_latency'] = df['latency_samples'].apply(lambda x: sorted(x)[len(x) // 2])
    df['std_dev'] = df['latency_samples'].apply(lambda x: np.std(x))

    df['latency_per_element_ns'] = (df['average_latency'] / df['vector_size']) * 1000

    return df


def generate_boxplot(df, file_name):
    communicators = sorted(df['communicator'].unique())
    vector_sizes = sorted(df['vector_size'].unique())

    all_samples = []
    positions = []
    communicator_list = []
    x_ticks = []
    x_labels = []
    width = 0.6
    box_width = width / len(communicators)
    offsets = np.linspace(-width/2 + box_width/2, width/2 - box_width/2, len(communicators))

    for idx, vector_size in enumerate(vector_sizes):
        for offset, communicator in zip(offsets, communicators):
            samples_series = df[(df['vector_size'] == vector_size) & (df['communicator'] == communicator)]['latency_samples']
            samples = [item for sublist in samples_series for item in sublist]
            all_samples.append(samples)
            positions.append(idx + 1 + offset)
            communicator_list.append(communicator)
        x_ticks.append(idx + 1)
        x_labels.append(str(vector_size))

    plt.figure(figsize=(12, 6))
    bplot = plt.boxplot(all_samples, 
        positions=positions, widths=box_width*0.9, 
        patch_artist=True, showfliers=False)

    colors = dict(zip(communicators, plt.cm.Blues(np.linspace(0.6, 1, len(communicators)))))
    for patch, communicator in zip(bplot['boxes'], communicator_list):
        patch.set_facecolor(colors[communicator])

    plt.xticks(x_ticks, x_labels)

    legend_handles = [Patch(facecolor=colors[communicator], 
        label=communicator) for communicator in communicators]
    plt.legend(handles=legend_handles)

    plt.title('ExchangeShares: Latency per Batch')
    plt.xlabel('Batch Size')
    plt.ylabel('Latency (µs)')
    plt.ylim(0)
    plt.tight_layout()

    plt.savefig(os.path.join(parent_dir, file_name))
    plt.close()


def generate_average_plot(df, file_name):
    communicators = sorted(df['communicator'].unique())

    plt.figure(figsize=(10, 6))

    for communicator in communicators:
        comm_data = df[df['communicator'] == communicator]
        
        plt.plot(comm_data['vector_size'], comm_data['average_latency'], 
                 label=f'{communicator}', marker='o')
        # plt.errorbar(comm_data['vector_size'], comm_data['average_latency'], 
        #              yerr=comm_data['std_dev'], label=f'{communicator}', marker='o', capsize=5)

    plt.title('ExchangeShares: Average Latency per Batch')
    plt.xlabel('Batch Size')
    plt.ylabel('Average Latency (µs)')
    
    plt.xscale("log", base=2)
    plt.xticks(df["vector_size"], labels=df["vector_size"])
    plt.ylim(0)
    plt.tight_layout()

    plt.legend(loc='upper left')

    plt.savefig(os.path.join(parent_dir, file_name))
    plt.close()


def generate_amortized_plot(df, file_name):
    communicators = sorted(df['communicator'].unique())

    plt.figure(figsize=(10, 6))

    for communicator in communicators:
        comm_data = df[df['communicator'] == communicator]
        
        plt.plot(comm_data['vector_size'], comm_data['latency_per_element_ns'], 
                 label=f'{communicator}', marker='o')

    plt.title('ExchangeShares: Average Latency per Element')
    plt.xlabel('Batch Size')
    plt.ylabel('Average Latency (ns)')
    
    plt.xscale("log", base=2)
    plt.xticks(df["vector_size"], labels=df["vector_size"])
    plt.ylim(0)
    plt.tight_layout()

    plt.legend(loc='upper left')

    plt.savefig(os.path.join(parent_dir, file_name))
    plt.close()


if __name__ == "__main__":

    df = parse_files()

    # (Optional) Filter vector_size range to make plots more readable
    df = df[(df["vector_size"] >= 2**13) & (df["vector_size"] <= 2**18)]

    # Generate plots
    generate_boxplot(df, 'boxplot.png')
    generate_average_plot(df, 'average_latency.png')
    generate_amortized_plot(df, 'latency_per_element.png')

    df.drop(columns=['latency_samples']).to_csv(os.path.join(parent_dir, 'batch-latency.csv'), index=False)
