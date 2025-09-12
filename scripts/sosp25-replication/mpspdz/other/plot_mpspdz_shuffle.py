import sys
import math
import re
from collections import defaultdict
import matplotlib.pyplot as plt


def read_secrecy_data(filename):
    rows_to_totalonline = defaultdict(list)
    current_rows = None
    
    # Regular expressions to capture rows and TotalOnline times
    rows_pattern = re.compile(r"^==== (\d+) rows;")
    totalonline_pattern = re.compile(r"\[=PROFILE\] +TotalOnline ([\d.]+) +sec")
    
    with open(filename, 'r') as file:
        for line in file:
            # Check if line indicates the number of rows
            rows_match = rows_pattern.match(line)
            if rows_match:
                current_rows = int(rows_match.group(1))
            
            # Check if line has a TotalOnline time and current rows is set
            totalonline_match = totalonline_pattern.match(line)
            if totalonline_match and current_rows is not None:
                totalonline_time = float(totalonline_match.group(1))
                rows_to_totalonline[current_rows].append(totalonline_time)
    
    avg_times = {int(math.log2(rows)): sum(times) / len(times) for rows, times in rows_to_totalonline.items()}
    return avg_times

def read_secrecy_preprocessed_perms(filename):
    exp_to_totalonline = defaultdict(list)
    current_exponent = None

    with open(filename, 'r') as file:
        for line in file:
            line = line.strip()
            if line.startswith("---"):
                # Reset for the next exponent
                current_exponent = None
            elif line.startswith("Iteration"):
                current_exponent = int(line.split()[-1])  # Extract exponent value
            elif line.startswith("Total execution time"):
                time = float(line.split()[-2])  # Extract execution time
                exp_to_totalonline[current_exponent].append(time)

    avg_times = {exp: sum(times) / len(times) for exp, times in exp_to_totalonline.items()}
    return avg_times

def read_mpspdz_data(filename):
    rows_to_totalonline = defaultdict(list)
    rows_to_totaloffline = defaultdict(list)

    # Regular expressions to match exponents and online times
    exponent_regex = re.compile(r"Iteration \d+: Exponent (\d+)")
    online_time_regex = re.compile(r"Spent ([0-9.]+) seconds")
    time_regex = re.compile(r"Spent ([0-9.]+) seconds .*? on the online phase and ([0-9.]+) seconds .*? on the preprocessing/offline phase")

    current_exponent = None

    with open(filename, 'r') as file:
        for line in file:
            # Check for a new exponent
            exponent_match = exponent_regex.search(line)
            if exponent_match:
                current_exponent = int(exponent_match.group(1))
                print(current_exponent)
                continue

            # Check for an online time (only if we are in an exponent's context)
            if current_exponent is not None:
                time_match = time_regex.search(line)
                if time_match:
                    online_time = float(time_match.group(1))
                    rows_to_totalonline[current_exponent].append(online_time)
                    offline_time = float(time_match.group(2))
                    rows_to_totaloffline[current_exponent].append(offline_time)
                else:
                    online_time_match = online_time_regex.search(line)
                    if online_time_match:
                        online_time = float(online_time_match.group(1))
                        rows_to_totalonline[current_exponent].append(online_time)

    print(rows_to_totalonline)
    avg_online_times = {rows: sum(times) / len(times) for rows, times in rows_to_totalonline.items()}
    avg_offline_times = {rows: sum(times) / len(times) for rows, times in rows_to_totaloffline.items()}
    return avg_online_times, avg_offline_times

def plot_data(num_parties, secrecy_data, preprocessing_data, mpspdz_data_online, mpspdz_data_offline, x_values=None, log=False):
    # Separate the keys (log input sizes) and values (average times)
    if x_values is None:
        x_values = sorted(secrecy_data.keys())
    y_values = [secrecy_data[x] for x in x_values]
    
    # Plotting
    plt.figure(figsize=(10, 6))
    plt.plot(x_values, y_values, marker='o', linestyle='-', color='b', label='Secrecy')

    if mpspdz_data_online is not None:
        y_values_2 = [mpspdz_data_online[x] for x in x_values]
        plt.plot(x_values, y_values_2, marker='o', linestyle='-', color='r', label='MP-SPDZ')
    
    if num_parties == 2:
        y_secrecy_preprocessing = [preprocessing_data[x] for x in x_values]
        plt.plot(x_values, y_secrecy_preprocessing, marker='o', linestyle='--', color='b', label='Secrecy (Preprocessing)')

        y_mpspdz_preprocessing = [mpspdz_data_offline[x] for x in x_values]
        plt.plot(x_values, y_mpspdz_preprocessing, marker='o', linestyle='--', color='r', label='MP-SPDZ (Preprocessing)')

    # Logarithmic x-axis label as powers of two
    plt.xticks(x_values, labels=[str(x) for  x in x_values])  # Format as powers of 2
    plt.ylim(bottom=0)
    if log:
        plt.ylim(bottom=1e-3)  # Ensure y-axis starts at 0
    plt.xlabel('Input Size (Power of 2)')
    plt.ylabel('Time (sec)')
    plt.title(f'Oblivious Shuffle ({num_parties}PC)')
    if mpspdz_data_online is not None:
        plt.legend()  # Add legend to show labels "Secrecy" and "MP-SPDZ"
    if log:
        plt.yscale('log')
    plt.grid(True, which="both", ls="--", linewidth=0.5)
    plt.show()

if __name__ == '__main__':
    log = False

    num_parties = int(sys.argv[1])

    secrecy_data = read_secrecy_data(sys.argv[2])

    mpspdz_data_online, mpspdz_data_offline = None, None
    if len(sys.argv) > 2:
        mpspdz_data_online, mpspdz_data_offline = read_mpspdz_data(sys.argv[3])
    
    preprocessing_data = None
    if num_parties == 2:
        preprocessing_data = read_secrecy_preprocessed_perms(sys.argv[4])
    
    # restriced x values
    start_exp = None
    end_exp = None
    index = 4
    x_values = None
    if num_parties == 2:
        index = 5
    if len(sys.argv) > index:
        start_exp = int(sys.argv[index])
        end_exp = int(sys.argv[index+1])
        x_values = list(range(start_exp, end_exp + 1))
    
    plot_data(num_parties, secrecy_data, preprocessing_data, mpspdz_data_online, mpspdz_data_offline, x_values, log)
