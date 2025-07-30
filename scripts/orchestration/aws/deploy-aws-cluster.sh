#!/bin/bash

# Check the number of arguments
# Example usage: ./deploy-aws-cluster.sh run_name 3 lan c7a.16xlarge
# Before use, set `AWS_ACCESS_KEY` and `AWS_SECRET_KEY`
# Or use `aws configure` to set up your credentials.
if [ $# -lt 1 ] || [ $# -gt 4 ]; then
    echo "Usage: $0 run_name [machines_num] [deploy_type] [machine_type]"
    exit 1
fi

cd $(dirname $0)

# Assign the provided arguments to variables or use default values
run_name=${1}                       # Used as a prefix for the spawn machines.
machines_num=${2:-3}                # Number of machines in the cluster.
deploy_type=${3:-lan}               # Either `lan` or `wan`.
machine_type=${4:-c7a.16xlarge}     # What type of AWS instance to use.


lan_openmpi_image="ami-05d5bb92233d842c8"
lan_openmpi_images=($lan_openmpi_image $lan_openmpi_image $lan_openmpi_image $lan_openmpi_image)
lan_assign_public_ips=(true false false false)
lan_regions=("us-east-2" "us-east-2" "us-east-2" "us-east-2")

# name - region - instance_type - image_id
machine_names=()
machine_regions=()
machine_types=()
machine_images=()
machine_public_ips=()

for i in $(seq 0 $(($machines_num-1)))
do
    machine_names+=("${run_name}_${i}")

    # update the machine_regions, machine_types, machine_images.
    if [ $deploy_type == "lan" ]; then
        machine_regions+=("${lan_regions[($i)%4]}")
        machine_types+=("${machine_type}")
        machine_images+=("${lan_openmpi_images[($i)%4]}")
        machine_public_ips+=(${lan_assign_public_ips[($i)%4]})
    else
        echo "deploy_type setting is not supported."
        exit 1
    fi
done


machine_json="["
for i in $(seq 0 $((${#machine_names[@]}-1)))
do
    machine_json+="{\"name\": \"${machine_names[$i]}\", \"region\": \"${machine_regions[$i]}\", \"instance_type\": \"${machine_types[$i]}\", \"image_id\": \"${machine_images[$i]}\", \"assign_public_ip\": ${machine_public_ips[$i]}}"
    if [ $i -lt $((${#machine_names[@]}-1)) ]; then
        machine_json+=","
    fi
done
machine_json+="]"

input_json="{\"machine_json\":${machine_json}}"

ansible-playbook ./common/run.yaml -e "${input_json}"
