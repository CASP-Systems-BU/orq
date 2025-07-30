#!/usr/bin/env bash

cd $(dirname $0)

RESULTS_DIR=$(pwd)/../../../results/secrecy/secrecy

echo "Extracting runtime results from log files..."

# Initialize result array
results=()

# exp_tpch_q6 - Extract time from "Time: X.XXXX" format
echo "Processing exp_tpch_q6.txt..."
if [[ -f "$RESULTS_DIR/exp_tpch_q6.txt" ]]; then
    time_val=$(grep "Time:" "$RESULTS_DIR/exp_tpch_q6.txt" | sed 's/Time: //' | tr -d ' ')
    if [[ -n "$time_val" ]]; then
        results+=("$time_val")
        # echo "  Found: $time_val"
    fi
fi

# Password Reuse - Extract last column (time)
echo "Processing exp_qpwd.txt..."
if [[ -f "$RESULTS_DIR/exp_qpwd.txt" ]]; then
    time_val=$(awk '{print $NF}' "$RESULTS_DIR/exp_qpwd.txt" | grep -E '^[0-9]+\.[0-9]+$')
    if [[ -n "$time_val" ]]; then
        results+=("$time_val")
        # echo "  Found: $time_val"
    fi
fi

# Credit Score - Extract last column (time)
echo "Processing exp_qcredit.txt..."
if [[ -f "$RESULTS_DIR/exp_qcredit.txt" ]]; then
    time_val=$(awk '{print $NF}' "$RESULTS_DIR/exp_qcredit.txt" | grep -E '^[0-9]+\.[0-9]+$')
    if [[ -n "$time_val" ]]; then
        results+=("$time_val")
        # echo "  Found: $time_val"
    fi
fi

# Comorbidity - Extract last column (time)
echo "Processing exp_q1.txt..."
if [[ -f "$RESULTS_DIR/exp_q1.txt" ]]; then
    time_val=$(awk '{print $NF}' "$RESULTS_DIR/exp_q1.txt" | grep -E '^[0-9]+\.[0-9]+$')
    if [[ -n "$time_val" ]]; then
        results+=("$time_val")
        # echo "  Found: $time_val"
    fi
fi

# Rec. cdiff - Extract last column (time)
echo "Processing exp_q2.txt..."
if [[ -f "$RESULTS_DIR/exp_q2.txt" ]]; then
    time_val=$(awk '{print $NF}' "$RESULTS_DIR/exp_q2.txt" | grep -E '^[0-9]+\.[0-9]+$')
    if [[ -n "$time_val" ]]; then
        results+=("$time_val")
        # echo "  Found: $time_val"
    fi
fi

# Aspirin count - Extract last column (time)
echo "Processing exp_q3.txt..."
if [[ -f "$RESULTS_DIR/exp_q3.txt" ]]; then
    time_val=$(awk '{print $NF}' "$RESULTS_DIR/exp_q3.txt" | grep -E '^[0-9]+\.[0-9]+$')
    if [[ -n "$time_val" ]]; then
        results+=("$time_val")
        # echo "  Found: $time_val"
    fi
fi

# exp_tpch_q4 - Extract last column (time)
echo "Processing exp_tpch_q4.txt..."
if [[ -f "$RESULTS_DIR/exp_tpch_q4.txt" ]]; then
    time_val=$(awk '{print $NF}' "$RESULTS_DIR/exp_tpch_q4.txt" | grep -E '^[0-9]+\.[0-9]+$')
    if [[ -n "$time_val" ]]; then
        results+=("$time_val")
        # echo "  Found: $time_val"
    fi
fi

# exp_tpch_q13 - Extract last column (time)
echo "Processing exp_tpch_q13.txt..."
if [[ -f "$RESULTS_DIR/exp_tpch_q13.txt" ]]; then
    time_val=$(awk '{print $NF}' "$RESULTS_DIR/exp_tpch_q13.txt" | grep -E '^[0-9]+\.[0-9]+$')
    if [[ -n "$time_val" ]]; then
        results+=("$time_val")
        # echo "  Found: $time_val"
    fi
fi

# Output results in requested format
echo ""
echo "Extracted runtime results:"
result_line=""
if [[ ${#results[@]} -gt 0 ]]; then
    # Format the results
    # result_line+="{"
    for i in "${!results[@]}"; do
        if [[ $i -eq 0 ]]; then
            result_line+="${results[$i]}"
        else
            result_line+=", ${results[$i]}"
        fi
    done
    # result_line+="}"
    
    # Print to console
    echo "$result_line"
    
    # Write to file
    echo "$result_line" > "$RESULTS_DIR/secrecy.csv"
    echo "Results written to $RESULTS_DIR/secrecy.csv"
else
    echo "No runtime values found."
fi