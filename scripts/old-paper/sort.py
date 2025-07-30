import subprocess
import shlex
import re
import sys
import matplotlib.pyplot as plt
import numpy as np

# Filenames of the test scripts
test_script = '../build/micro_sorting'

num_parties = int(sys.argv[1])
#machines = ','.join([f'node{i}' for i in range(num_parties)]) 
machines = ','.join([f'node{i}' for i in range(num_parties)]) 

# Input sizes to test
input_sizes = [1000, 10000, 100000, 1000000, 10000000, 50000000]

batch_size = 65536

# Number of times to run each test
num_runs = 3

# Output file to write results
output_file = '../results/' + sys.argv[2] + '.txt'
output_image = '../results/' + sys.argv[2] + '.png'
output_pie = '../results/' + sys.argv[2] + '-pie.png'

local = False
if len(sys.argv) > 3:
    if sys.argv[3] == 'local':
        local = True

# Regular expression to parse the output# Regular expressions to parse the output
sort_pattern = re.compile(r'\[ SW\]\s+(\w+)\s+([\d.]+)\s+sec')
profile_pattern = re.compile(r'\[=PROFILE\]\s+(\w+)\s+([\d.]+)\s+sec')

# Data structures to store results
quicksort_times = {}
radixsort_times = {}
profile_data = {}

with open(output_file, 'w') as f:
    for size in input_sizes:
        print('Input size: ', size)
        quicksort_times[size] = []
        radixsort_times[size] = []
        profile_data[size] = {}
        for run in range(num_runs):
            # Run the script with the input size
            if num_parties == 1:
                command = f'{test_script} 1 1 {batch_size} {size}'
            elif local:
                command = f'mpirun -np {num_parties} {test_script} 1 1 {batch_size} {size}'
            else:
                command = f'mpirun -np {num_parties} --host {machines} {test_script} 1 1 {batch_size} {size}'
            result = subprocess.run(shlex.split(command), capture_output=True, text=True)
            
            # Write the output to the file
            f.write(f"Script: {test_script}, Input size: {size}, Run: {run + 1}\n")
            f.write(result.stdout)
            f.write("\n")
            
            # Parse the output
            sort_matches = sort_pattern.findall(result.stdout)
            profile_matches = profile_pattern.findall(result.stdout)
            
            for match in sort_matches:
                algorithm, time = match
                print(algorithm, time)
                if 'Quicksort' in algorithm:
                    quicksort_times[size].append(float(time))
                elif 'Radixsort' in algorithm:
                    radixsort_times[size].append(float(time))
            
            # Process profile data
            run_profile = {}
            for match in profile_matches:
                profile_name, time = match
                run_profile[profile_name] = float(time)
                if profile_name not in profile_data[size]:
                    profile_data[size][profile_name] = []
                profile_data[size][profile_name].append(float(time))
            

quicksort_averages = {size: sum(times) / len(times) for size, times in quicksort_times.items()}
radixsort_averages = {size: sum(times) / len(times) for size, times in radixsort_times.items()}
# Calculate average profile data
profile_averages = {size: {name: sum(times) / len(times) for name, times in profiles.items()} 
                    for size, profiles in profile_data.items()}

'''
LINE CHART
'''

# Plotting
plt.figure(figsize=(10, 6))
plt.plot(input_sizes, [quicksort_averages[size] for size in input_sizes], label='Quicksort', marker='o')
plt.plot(input_sizes, [radixsort_averages[size] for size in input_sizes], label='Radixsort', marker='o')
plt.xlabel('Input Size')
plt.ylabel('Average Time (sec)')
plt.title('Quicksort vs Radixsort Performance')
plt.legend()
plt.grid(True)
plt.xscale('log')  # Use logarithmic scale for input sizes
plt.yscale('log')  # Use logarithmic scale for times
plt.savefig(output_image, format='png', dpi=300, bbox_inches='tight')

'''
PIE CHART
'''

# Prepare data for plotting
profile_components_quicksort = ['LocalQ', 'Shuffle', 'Comparisons', 'Open']
profile_components_radixsort = ['LocalR', 'B2A_bit', 'ApplyPerm']
def prepare_plot_data(sort_averages, profile_averages, profile_components):
    size = input_sizes[-1]
    total_time = sort_averages[size]
    profile_times = [profile_averages[size].get(component, 0) for component in profile_components]
    # Adjust the last component to make sure the sum matches the total time
    profile_times[-1] = total_time - sum(profile_times[:-1])
    data = profile_times
    return np.array(data)

# Create a figure for the pie charts
quicksort_profile_times = prepare_plot_data(quicksort_averages, profile_averages, profile_components_quicksort)
radixsort_profile_times = prepare_plot_data(radixsort_averages, profile_averages, profile_components_radixsort)

# Define colors for the pie charts
quicksort_colors = ['#B9D9EB', '#00CED1', '#0CAFFF', '#3F00FF']  # Shades of blue
radixsort_colors = ['#CCCCFF', '#CBC3E3', '#CF9FFF']  # Shades of purple

fig, axs = plt.subplots(1, 2, figsize=(12, 6))
# Plotting the pie chart for quicksort
wedges1, _ = axs[0].pie(quicksort_profile_times, startangle=90, colors=quicksort_colors)
axs[0].set_title('Quicksort Profile')

# Plotting the pie chart for radixsort
wedges2, _ = axs[1].pie(radixsort_profile_times, startangle=90, colors=radixsort_colors)
axs[1].set_title('Radixsort Profile')

# Create legends for each pie chart
# Calculate percentages for quicksort
quicksort_percentages = [f'{(time / sum(quicksort_profile_times)) * 100:.1f}%' for time in quicksort_profile_times]
# Create legend for quicksort with percentages
axs[0].legend(wedges1, [f'{comp}: {perc}' for comp, perc in zip(profile_components_quicksort, quicksort_percentages)],
               title="Quicksort Components", loc="center left", bbox_to_anchor=(1, 0, 0.5, 1))

# Calculate percentages for radixsort
radixsort_percentages = [f'{(time / sum(radixsort_profile_times)) * 100:.1f}%' for time in radixsort_profile_times]
# Create legend for radixsort with percentages
axs[1].legend(wedges2, [f'{comp}: {perc}' for comp, perc in zip(profile_components_radixsort, radixsort_percentages)],
               title="Radixsort Components", loc="center left", bbox_to_anchor=(1, 0, 0.5, 1))

# Equal aspect ratio ensures that pie charts are circular
for ax in axs:
    ax.axis('equal')

# Show the plot
plt.tight_layout()
plt.savefig('../results/ahhhhhh.png', format='png', dpi=300)