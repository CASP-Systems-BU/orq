cd ~/
sudo apt update && sudo apt upgrade -y
mkdir results
cd results
mkdir sorting-main
cd ~/

./orq/scripts/orchestration/deploy.sh ~/orq/ node0 node1 node2 node3

cd ~/orq
mkdir build
cd build
../scripts/setup.sh

# max size (power of two)
DEFAULT_MAX_SIZE=27
MAX_SIZE=${1:-$DEFAULT_MAX_SIZE}
echo "Max power of two: $MAX_SIZE"

# run cmake once to set the triples to zeroes
cmake .. -DPROTOCOL=2 -DTRIPLES=ZERO

../scripts/run_experiment.sh -p 2 -s lan -c nocopy -n 8 -r 20-$MAX_SIZE -T 34 -b -12 -e 3 -x node ./micro_sorting >> ~/results/sorting-main/2pc.txt
../scripts/run_experiment.sh -p 3 -s lan -c nocopy -n 8 -r 20-$MAX_SIZE -T 34 -b -12 -e 3 -x node ./micro_sorting >> ~/results/sorting-main/3pc.txt
../scripts/run_experiment.sh -p 4 -s lan -c nocopy -n 8 -r 20-$MAX_SIZE -T 34 -b -12 -e 3 -x node -m -DUSE_DALSKOV_FANTASTIC_FOUR=ON ./micro_sorting >> ~/results/sorting-main/4pc.txt
cmake .. -DPROTOCOL=4 -DUSE_DALSKOV_FANTASTIC_FOUR=OFF

./run-plot.sh $MAX_SIZE
