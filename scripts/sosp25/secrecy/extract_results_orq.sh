#!/usr/bin/env bash

cd $(dirname $0)

RESULTS_DIR=$(pwd)/../../../results/secrecy/orq

echo "Extracting runtime results from log files..."

# Initialize result array
results=()

# exp_tpch_q6 
echo "Processing exp_tpch_q6.txt..."
if [[ -f "$RESULTS_DIR/exp_tpch_q6.txt" ]]; then
    runtime=$(grep "\[=SW\].*Overall" "$RESULTS_DIR/exp_tpch_q6.txt" | grep -oP '\d+\.\d+(?=\s+sec)')
    if [[ -n "$runtime" ]]; then
        results+=("$runtime")
        echo "  Found runtime: $runtime seconds"
    else
        echo "  No runtime found in exp_tpch_q6.txt"
    fi
else
    echo "  exp_tpch_q6.txt not found"
fi

# Password Reuse
echo "Processing exp_qpwd.txt..."
if [[ -f "$RESULTS_DIR/exp_qpwd.txt" ]]; then
    runtime=$(grep "\[=SW\].*Overall" "$RESULTS_DIR/exp_qpwd.txt" | grep -oP '\d+\.\d+(?=\s+sec)')
    if [[ -n "$runtime" ]]; then
        results+=("$runtime")
        echo "  Found runtime: $runtime seconds"
    else
        echo "  No runtime found in exp_qpwd.txt"
    fi
else
    echo "  exp_qpwd.txt not found"
fi

# Credit Score
echo "Processing exp_qcredit.txt..."
if [[ -f "$RESULTS_DIR/exp_qcredit.txt" ]]; then
    runtime=$(grep "\[=SW\].*Overall" "$RESULTS_DIR/exp_qcredit.txt" | grep -oP '\d+\.\d+(?=\s+sec)')
    if [[ -n "$runtime" ]]; then
        results+=("$runtime")
        echo "  Found runtime: $runtime seconds"
    else
        echo "  No runtime found in exp_qcredit.txt"
    fi
else
    echo "  exp_qcredit.txt not found"
fi

# Comorbidity 
echo "Processing exp_q1.txt..."
if [[ -f "$RESULTS_DIR/exp_q1.txt" ]]; then
    runtime=$(grep "\[=SW\].*Overall" "$RESULTS_DIR/exp_q1.txt" | grep -oP '\d+\.?\d+(?=\s+sec)')
    if [[ -n "$runtime" ]]; then
        results+=("$runtime")
        echo "  Found runtime: $runtime seconds"
    else
        echo "  No runtime found in exp_q1.txt"
    fi
else
    echo "  exp_q1.txt not found"
fi

# Rec. cdiff
echo "Processing exp_q2.txt..."
if [[ -f "$RESULTS_DIR/exp_q2.txt" ]]; then
    runtime=$(grep "\[=SW\].*Overall" "$RESULTS_DIR/exp_q2.txt" | grep -oP '\d+\.\d+(?=\s+sec)')
    if [[ -n "$runtime" ]]; then
        results+=("$runtime")
        echo "  Found runtime: $runtime seconds"
    else
        echo "  No runtime found in exp_q2.txt"
    fi
else
    echo "  exp_q2.txt not found"
fi

# Aspirin count 
echo "Processing exp_q3.txt..."
if [[ -f "$RESULTS_DIR/exp_q3.txt" ]]; then
    runtime=$(grep "\[=SW\].*Overall" "$RESULTS_DIR/exp_q3.txt" | grep -oP '\d+\.\d+(?=\s+sec)')
    if [[ -n "$runtime" ]]; then
        results+=("$runtime")
        echo "  Found runtime: $runtime seconds"
    else
        echo "  No runtime found in exp_q3.txt"
    fi
else
    echo "  exp_q3.txt not found"
fi

# exp_tpch_q4
echo "Processing exp_tpch_q4.txt..."
if [[ -f "$RESULTS_DIR/exp_tpch_q4.txt" ]]; then
    runtime=$(grep "\[=SW\].*Overall" "$RESULTS_DIR/exp_tpch_q4.txt" | grep -oP '\d+\.\d+(?=\s+sec)')
    if [[ -n "$runtime" ]]; then
        results+=("$runtime")
        echo "  Found runtime: $runtime seconds"
    else
        echo "  No runtime found in exp_tpch_q4.txt"
    fi
else
    echo "  exp_tpch_q4.txt not found"
fi

# exp_tpch_q13
echo "Processing exp_tpch_q13.txt..."
if [[ -f "$RESULTS_DIR/exp_tpch_q13.txt" ]]; then
    runtime=$(grep "\[=SW\].*Overall" "$RESULTS_DIR/exp_tpch_q13.txt" | grep -oP '\d+\.\d+(?=\s+sec)')
    if [[ -n "$runtime" ]]; then
        results+=("$runtime")
        echo "  Found runtime: $runtime seconds"
    else
        echo "  No runtime found in exp_tpch_q13.txt"
    fi
else
    echo "  exp_tpch_q13.txt not found"
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
    echo "$result_line" > "$RESULTS_DIR/orq.csv"
    echo "Results written to $RESULTS_DIR/orq.csv"
else
    echo "No runtime values found."
fi