#!/usr/bin/env bash

# We need this script on AWS to make sure the machines know each other's IP addresses.
# ./_update_hostfile.sh -x node -i 
usage () {
    echo "Usage: ${0} [options]"
    echo "Example: ${0} -x node -i 1.0.0.1,1.0.0.2"
    echo "OPTIONS:"
    echo "  [-h]                                    Show this help"
    echo "  [-i]                                    List of IP addresses of the remote nodes."
    echo "  [-x node prefix]                        Prefix for remote nodes. Default is 'node'."
    exit 1
}

(( $# < 1 )) && usage

# Defaults
node_prefix="node"

while getopts "hi:x:" opt; do
    case ${opt} in
        h)
            usage
            ;;
        i)
            ip_addresses=(${OPTARG})
            ;;
        x)
            node_prefix=${OPTARG}
            ;;
        \?)
            echo "Invalid option: $OPTARG" 1>&2
            ;;
    esac
done

# Update /etc/hosts
sudo sh -c '
echo "127.0.0.1 localhost

# The following lines are desirable for IPv6 capable hosts
::1 ip6-localhost ip6-loopback
fe00::0 ip6-localnet
ff00::0 ip6-mcastprefix
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters
ff02::3 ip6-allhosts

" > /etc/hosts
'

# Write IP addresses to /etc/hosts with the node prefix + 0-index
# For example, ip_addresses = "18.188.23.207,18.188.23.212"
# First split the string into an array of IP addresses
ip_addresses_list=$(echo $ip_addresses | tr "," "\n")


# Then adds the ips to file such as:
# "18.188.23.207 node0"
# "18.188.23.212 node1"
index=0
for ip in $ip_addresses_list; do
    sudo sh -c "echo $ip $node_prefix$index >> /etc/hosts"  
    ssh-keygen -R $node_prefix$index
    index=$((index+1))
done


# Copy the updated /etc/hosts to all nodes
for ip in $ip_addresses_list; do
    scp -o StrictHostKeyChecking=no /etc/hosts $ip:/tmp/hosts
    ssh -o StrictHostKeyChecking=no $ip "sudo mv /tmp/hosts /etc/hosts"
done
