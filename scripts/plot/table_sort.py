import subprocess
import shlex
import re
import sys
import matplotlib.pyplot as plt
import numpy as np

# Filenames of the test scripts
test_script = '../../build/micro_tablesort'

num_parties = int(sys.argv[1]) 
machines = ','.join([f'node{i}' for i in range(num_parties)]) 

local = False
if len(sys.argv) > 3:
    if sys.argv[3] == 'local':
        local = True

input_size = 12 # power of two
batch_size = 65536
# Number of times to run each test
num_runs = 3

# Output file to write results
output_file = '../bench-results/' + sys.argv[2] + '.txt'
output_image_single = '../bench-results/' + sys.argv[2] + '-single.png'
output_image_single_quick = '../bench-results/' + sys.argv[2] + '-single-quick.png'
output_image_single_radix = '../bench-results/' + sys.argv[2] + '-single-radix.png'
output_image_multi = '../bench-results/' + sys.argv[2] + '-multi.png'
output_image_multi_quick = '../bench-results/' + sys.argv[2] + '-multi-quick.png'
output_image_multi_radix = '../bench-results/' + sys.argv[2] + '-multi-radix.png'

regex_pattern = re.compile(r'\[ SW\] +Table +(\w+) +([\d.]+) +sec|\[=PROFILE\] +(.+?) +([\d.]+) +sec')

num_columns = [1, 2, 3, 4]

'''
1 sort column, 1 through 4 columns
'''
bitonicsort_single_times = {}
quicksort_single_times = {}
radixsort_single_times = {}

quicksort_single_profiling = {}
radixsort_single_profiling = {}

with open(output_file, 'w') as f:
    for i in num_columns:
        bitonicsort_single_times[i] = []
        quicksort_single_times[i] = []
        radixsort_single_times[i] = []
        
        quicksort_single_profiling[i] = []
        radixsort_single_profiling[i] = []

        for run in range(num_runs):
            quicksort_single_profiling[i].append({})
            radixsort_single_profiling[i].append({})

            # Run the script with the input size
            if num_parties == 1:
                command = f'{test_script} 1 1 {batch_size} {input_size} {i} 1'
            elif local:
                command = f'mpirun -np {num_parties} {test_script} 1 1 {batch_size} {input_size} {i} 1'
            else:
                command = f'mpirun -np {num_parties} --host {machines} {test_script} 1 1 {batch_size} {input_size} {i} 1'
            result = subprocess.run(shlex.split(command), capture_output=True, text=True)
            
            # Write the output to the file
            f.write(f"Script: {test_script}, Input size: {input_size}, Run: {run + 1}\n")
            f.write(result.stdout)
            f.write("\n")

            sort_matches = regex_pattern.findall(result.stdout)

            radixsort = False

            for match in sort_matches:
                if match[0]:  # This means it's a sort result
                    algorithm, time = match[0], match[1]
                    print(algorithm, time)
                    
                    # Store the time in the respective dictionary
                    if algorithm == "Bitonicsort":
                        bitonicsort_single_times[i].append(float(time))
                    elif algorithm == "Quicksort":
                        quicksort_single_times[i].append(float(time))
                        radixsort = True
                    elif algorithm == "Radixsort":
                        radixsort_single_times[i].append(float(time))
                elif match[2]:  # This means it's a profiling result
                    profile_name, profile_time = match[2], match[3]
                    if not radixsort:
                        quicksort_single_profiling[i][run][profile_name] = float(profile_time)
                    elif radixsort:
                        radixsort_single_profiling[i][run][profile_name] = float(profile_time)

bitonicsort_single_averages = {size: sum(times) / len(times) for size, times in bitonicsort_single_times.items()}
quicksort_single_averages = {size: sum(times) / len(times) for size, times in quicksort_single_times.items()}
radixsort_single_averages = {size: sum(times) / len(times) for size, times in radixsort_single_times.items()}

def calculate_averages(profiling_data):
    averages = {}
    for key, runs in profiling_data.items():
        averages[key] = {}
        for run in runs:
            for metric, value in run.items():
                if metric not in averages[key]:
                    averages[key][metric] = []
                averages[key][metric].append(value)

        # Calculate the average for each metric
        for metric, values in averages[key].items():
            averages[key][metric] = sum(values) / len(values)

    return averages

quicksort_single_profiling_averages = calculate_averages(quicksort_single_profiling)
radixsort_single_profiling_averages = calculate_averages(radixsort_single_profiling)

# plot the comparison between protocols (line graph)
plt.figure(figsize=(10, 6))
plt.plot(num_columns, [bitonicsort_single_averages[size] for size in num_columns], label='Bitonicsort', marker='o')
plt.plot(num_columns, [quicksort_single_averages[size] for size in num_columns], label='Quicksort', marker='o')
plt.plot(num_columns, [radixsort_single_averages[size] for size in num_columns], label='Radixsort', marker='o')
plt.xlabel('Number of Columns')
plt.ylabel('Average Time (sec)')
plt.title('Table Sort Performance (16m)')
plt.legend()
plt.grid(True)
plt.savefig(output_image_single, format='png', dpi=300, bbox_inches='tight')

# plot the quicksort stacked bar graph
categories = list(quicksort_single_profiling_averages[1].keys())
num_columns = list(quicksort_single_profiling_averages.keys())
values = np.array([[quicksort_single_profiling_averages[col][cat] for cat in categories] for col in num_columns])
# Create the stacked bar plot
plt.figure(figsize=(10, 6))
bottoms = np.zeros(len(num_columns))

for i, category in enumerate(categories):
    plt.bar(num_columns, values[:, i], label=category, bottom=bottoms)
    bottoms += values[:, i]

# Add labels and title
plt.xlabel('Number of Columns')
plt.ylabel('Average Time (sec)')
plt.title('Quicksort Profiling Averages, Variable Columns')
plt.xticks(num_columns)  # Set x-ticks to be the number of columns
plt.legend(title='Profiling Metrics')
plt.tight_layout()
plt.savefig(output_image_single_quick, format='png', dpi=300, bbox_inches='tight')

# plot the radixsort stacked bar graph
categories = list(radixsort_single_profiling_averages[1].keys())
num_columns = list(radixsort_single_profiling_averages.keys())
values = np.array([[radixsort_single_profiling_averages[col][cat] for cat in categories] for col in num_columns])
# Create the stacked bar plot
plt.figure(figsize=(10, 6))
bottoms = np.zeros(len(num_columns))

for i, category in enumerate(categories):
    plt.bar(num_columns, values[:, i], label=category, bottom=bottoms)
    bottoms += values[:, i]

# Add labels and title
plt.xlabel('Number of Columns')
plt.ylabel('Average Time (sec)')
plt.title('Radixsort Profiling Averages, Variable Columns')
plt.xticks(num_columns)  # Set x-ticks to be the number of columns
plt.legend(title='Profiling Metrics')
plt.tight_layout()
plt.savefig(output_image_single_radix, format='png', dpi=300, bbox_inches='tight')




'''
4 columns, 1 through 4 sort columns
'''
bitonicsort_multi_times = {}
quicksort_multi_times = {}
radixsort_multi_times = {}

quicksort_multi_profiling = {}
radixsort_multi_profiling = {}

with open(output_file, 'w') as f:
    for i in num_columns:
        bitonicsort_multi_times[i] = []
        quicksort_multi_times[i] = []
        radixsort_multi_times[i] = []

        quicksort_multi_profiling[i] = []
        radixsort_multi_profiling[i] = []

        for run in range(num_runs):
            quicksort_multi_profiling[i].append({})
            radixsort_multi_profiling[i].append({})

            # Run the script with the input size
            if num_parties == 1:
                command = f'{test_script} 1 1 {batch_size} {input_size} 4 {i}'
            elif local:
                command = f'mpirun -np {num_parties} {test_script} 1 1 {batch_size} {input_size} 4 {i}'
            else:
                command = f'mpirun -np {num_parties} --host {machines} {test_script} 1 1 {batch_size} {input_size} 4 {i}'
            result = subprocess.run(shlex.split(command), capture_output=True, text=True)
            
            # Write the output to the file
            f.write(f"Script: {test_script}, Input size: {input_size}, Run: {run + 1}\n")
            f.write(result.stdout)
            f.write("\n")

            sort_matches = regex_pattern.findall(result.stdout)

            radixsort = False

            for match in sort_matches:
                if match[0]:  # This means it's a sort result
                    algorithm, time = match[0], match[1]
                    print(algorithm, time)
                    
                    # Store the time in the respective dictionary
                    if algorithm == "Bitonicsort":
                        bitonicsort_multi_times[i].append(float(time))
                    elif algorithm == "Quicksort":
                        quicksort_multi_times[i].append(float(time))
                        radixsort = True
                    elif algorithm == "Radixsort":
                        radixsort_multi_times[i].append(float(time))
                elif match[2]:  # This means it's a profiling result
                    profile_name, profile_time = match[2], match[3]
                    if not radixsort:
                        quicksort_multi_profiling[i][run][profile_name] = float(profile_time)
                    elif radixsort:
                        radixsort_multi_profiling[i][run][profile_name] = float(profile_time)


bitonicsort_multi_averages = {size: sum(times) / len(times) for size, times in bitonicsort_multi_times.items()}
quicksort_multi_averages = {size: sum(times) / len(times) for size, times in quicksort_multi_times.items()}
radixsort_multi_averages = {size: sum(times) / len(times) for size, times in radixsort_multi_times.items()}

quicksort_multi_profiling_averages = calculate_averages(quicksort_multi_profiling)
radixsort_multi_profiling_averages = calculate_averages(radixsort_multi_profiling)

# plot the comparison between protocols (line graph)
plt.figure(figsize=(10, 6))
plt.plot(num_columns, [bitonicsort_multi_averages[size] for size in num_columns], label='Bitonicsort', marker='o')
plt.plot(num_columns, [quicksort_multi_averages[size] for size in num_columns], label='Quicksort', marker='o')
plt.plot(num_columns, [radixsort_multi_averages[size] for size in num_columns], label='Radixsort', marker='o')
plt.xlabel('Number of Sort Columns')
plt.ylabel('Average Time (sec)')
plt.title('Table Sort Performance (16m, 4 total columns)')
plt.legend()
plt.grid(True)
plt.savefig(output_image_multi, format='png', dpi=300, bbox_inches='tight')

# plot the quicksort stacked bar graph
categories = list(quicksort_multi_profiling_averages[1].keys())
num_columns = list(quicksort_multi_profiling_averages.keys())
values = np.array([[quicksort_multi_profiling_averages[col][cat] for cat in categories] for col in num_columns])
# Create the stacked bar plot
plt.figure(figsize=(10, 6))
bottoms = np.zeros(len(num_columns))

for i, category in enumerate(categories):
    plt.bar(num_columns, values[:, i], label=category, bottom=bottoms)
    bottoms += values[:, i]

# Add labels and title
plt.xlabel('Number of Sort Columns')
plt.ylabel('Average Time (sec)')
plt.title('Quicksort Profiling Averages, Variable Sort Columns')
plt.xticks(num_columns)  # Set x-ticks to be the number of columns
plt.legend(title='Profiling Metrics')
plt.tight_layout()
plt.savefig(output_image_multi_quick, format='png', dpi=300, bbox_inches='tight')

# plot the radixsort stacked bar graph
categories = list(radixsort_multi_profiling_averages[1].keys())
num_columns = list(radixsort_multi_profiling_averages.keys())
values = np.array([[radixsort_multi_profiling_averages[col][cat] for cat in categories] for col in num_columns])
# Create the stacked bar plot
plt.figure(figsize=(10, 6))
bottoms = np.zeros(len(num_columns))

for i, category in enumerate(categories):
    plt.bar(num_columns, values[:, i], label=category, bottom=bottoms)
    bottoms += values[:, i]

# Add labels and title
plt.xlabel('Number of Sort Columns')
plt.ylabel('Average Time (sec)')
plt.title('Radixsort Profiling Averages, Variable Sort Columns')
plt.xticks(num_columns)  # Set x-ticks to be the number of columns
plt.legend(title='Profiling Metrics')
plt.tight_layout()
plt.savefig(output_image_multi_radix, format='png', dpi=300, bbox_inches='tight')
