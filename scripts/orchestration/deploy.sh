#!/usr/bin/env bash
# cluster deploy script. call with a list of all nodes on the command line,
# with the main node as the first arg.
#
# assumes you can currently access other nodes via ssh forwarding, but will
# create a new key and copy over to all others.
#
# example:
# $ deploy.sh repo-path node{0,1,2,3}

if [[ $# -lt 3 ]]; then
    echo "Usage: $0 repo-path node0 [node1 ...]"
    exit 1
fi

echo "Checking for public key..."
if ! ls ~/.ssh/*.pub; then
    echo "No public key! Generate new."
    ssh-keygen -t ed25519
fi

REPO_NAME=$1
shift
echo "Using repo $REPO_NAME"

cd $REPO_NAME || exit 1

ALL_NODES=$(python3 -c "print(','.join(input().split()))" <<< $@)
NUM_NODES=$#

echo "== $NUM_NODES nodes: $ALL_NODES"

MAIN_NODE=$1
echo "Main node is $MAIN_NODE"
shift

# Required by the `startmpc` script
ssh-copy-id -o StrictHostKeyChecking=no -i ~/.ssh/*.pub $MAIN_NODE

for W in "$@"; do
    echo "== Minimal setup on node $W..."
    ssh-copy-id -o StrictHostKeyChecking=no -i ~/.ssh/*.pub $W
    
    # Run in the background
    (scp -r $REPO_NAME/ $W:~/; ssh $W mkdir -p $REPO_NAME/build; ssh $W $REPO_NAME/scripts/_setup_required.sh) &
    SSH_PIDS+=($!)
done

for pid in "${SSH_PIDS[@]}"; do
    wait $pid
    echo "Remote PID $pid done"
done

mkdir -p build
cd build

# setup on the host
../scripts/setup.sh

echo 'Testing MPI execution of `example2`...'
cmake .. -DPROTOCOL=$NUM_NODES -DCOMM=MPI &&
make -j $(nproc) example2

for W in "$@"; do
    scp example2 $W:$REPO_NAME/build
done

mpirun --host $ALL_NODES -np $NUM_NODES example2 &&
echo 'Installation complete.'