# scripts/sort-graphs.py
import matplotlib.pyplot as plt
import numpy as np

threads = [1, 2, 4, 8, 16]
thread_labels = ["1", "2", "4", "8", "16"]

q_results = {thread: [] for thread in threads}  # Initialize results for each thread
r_results = {thread: [] for thread in threads}  # Initialize results for each thread
q_profiling = {thread: [] for thread in threads}
r_profiling = {thread: [] for thread in threads}

def parse_sorting_logs(file_path):
    with open(file_path, 'r') as file:
        data = file.read()

    blocks = data.split('STARTING-')

    for block in blocks:
        if block.strip():  # Skip empty blocks
            lines = block.strip().splitlines()
            header = lines[0].strip()  # Get the header line
            if "SORT" not in header:
                continue
            algorithm, thread_info = header.split(', THREADS=')
            threads = int(thread_info.strip())  # Extract the number of threads

            profile_map = {}

            for line in lines[1:]:
                if line.startswith('[ SW]'):
                    total_time = float(line.split()[-2])
                    if algorithm == 'QUICKSORT':
                        q_results[threads].append(total_time)
                    if algorithm == 'RADIXSORT':
                        r_results[threads].append(total_time)
                elif line.startswith('[=PROFILE]'):
                    profile_data = line.split()
                    profile_name = profile_data[1]
                    profile_time = float(profile_data[-2])
                    profile_map[profile_name] = profile_time
            
            if algorithm == 'QUICKSORT':
                q_profiling[threads].append(profile_map)
            elif algorithm == 'RADIXSORT':
                r_profiling[threads].append(profile_map)

def compute_average_profiles(profiling_data):
    averages = {}
    for thread, profiles in profiling_data.items():
        if profiles:  # Check if there are profiles to average
            profile_sums = {}
            for profile in profiles:
                for name, time in profile.items():
                    if name not in profile_sums:
                        profile_sums[name] = []
                    profile_sums[name].append(time)

            # Calculate averages
            averages[thread] = {name: sum(times) / len(times) for name, times in profile_sums.items()}
    return averages

# plot the radixsort stacked bar graph
def plot_stacked_bars(q_profile_averages, outfile, algorithm):
    colors = plt.get_cmap('Pastel1')
    categories = list(q_profile_averages[1].keys())
    num_columns = list(q_profile_averages.keys())
    values = np.array([[q_profile_averages[col][cat] for cat in categories] for col in num_columns])
    # Create the stacked bar plot
    plt.figure(figsize=(10, 6))
    bottoms = np.zeros(len(num_columns))

    for i, category in enumerate(categories):
        plt.bar(thread_labels, values[:, i], label=category, bottom=bottoms, color=colors(i))
        bottoms += values[:, i]

    # Add labels and title
    plt.xlabel('Number of Threads')
    plt.ylabel('Average Time (sec)')
    plt.title(algorithm+' Profiling Averages')
    plt.legend(title='Profiling Metrics')
    plt.tight_layout()
    plt.savefig(outfile, format='png', dpi=300, bbox_inches='tight')

def plot_ideal_comparison(q_averages, r_averages, outfile, quicksort_only, radixsort_only):
    plt.figure(figsize=(10, 6))
    # get larger of the two max times
    max_time = 0
    if quicksort_only:
        max_time = q_averages[1]
    elif radixsort_only:
        max_time = r_averages[1]
    else:
        max_time = q_averages[1] if q_averages[1] >= r_averages[1] else r_averages[1]
    ideal_times = [max_time / t for t in threads]
    if not radixsort_only:
        plt.plot(thread_labels, [q_averages[size] for size in threads], label='Quicksort', marker='o')
    if not quicksort_only:
        plt.plot(thread_labels, [r_averages[size] for size in threads], label='Radixsort', marker='o')
    plt.plot(thread_labels, ideal_times, label='Ideal', marker='o', color='gray', linestyle='--')
    plt.xlabel('Number of Threads')
    plt.ylabel('Average Time (sec)')
    plt.title('Parallel Sort Performance (8m)')
    plt.legend()
    plt.grid(True)
    plt.savefig(outfile, format='png', dpi=300, bbox_inches='tight')


import sys
if __name__ == "__main__":
    input_file_path = sys.argv[1]
    output_path = sys.argv[2]
    quicksort_only = False
    radixsort_only = False
    if len(sys.argv) > 3:
        if sys.argv[3] == "quick":
            quicksort_only = True
        if sys.argv[3] == "radix":
            radixsort_only = True
    parse_sorting_logs(input_file_path)
    q_averages = None
    r_averages = None
    # Calculate average profile data
    if not radixsort_only:
        q_averages = {threads: sum(times) / len(times) for threads, times in q_results.items()}
        q_profile_averages = compute_average_profiles(q_profiling)
        plot_stacked_bars(q_profile_averages, output_path+"quick.png", "Quicksort")
    if not quicksort_only:
        r_averages = {threads: sum(times) / len(times) for threads, times in r_results.items()}
        r_profile_averages = compute_average_profiles(r_profiling)
        plot_stacked_bars(r_profile_averages, output_path+"radix.png", "Radixsort")

    plot_ideal_comparison(q_averages, r_averages, output_path+"overall.png", quicksort_only, radixsort_only)
