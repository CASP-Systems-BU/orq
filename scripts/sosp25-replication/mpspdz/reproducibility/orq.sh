cd ~/
sudo apt update && sudo apt upgrade -y
mkdir results

./orq/scripts/orchestration/deploy.sh ~/orq/ node0 node1 node2 node3

cd ~/orq
mkdir build
cd build
../scripts/setup.sh

# run cmake once to set the triples to zeroes
cmake .. -DPROTOCOL=2 -DTRIPLES=ZERO

../scripts/run_experiment.sh -p 2 -s lan -c nocopy -n 17 -r 16-19 -T 17 -b 16384 -e 3 -x node ./micro_sorting > ~/results/orq-2pc.txt
../scripts/run_experiment.sh -p 2 -s lan -c nocopy -n 8 -r 20-22 -T 34 -b -12 -e 3 -x node ./micro_sorting >> ~/results/orq-2pc.txt

../scripts/run_experiment.sh -p 3 -s lan -c nocopy -n 17 -r 16-19 -T 17 -b 16384 -e 3 -x node ./micro_sorting > ~/results/orq-3pc.txt
../scripts/run_experiment.sh -p 3 -s lan -c nocopy -n 8 -r 20-25 -T 34 -b -12 -e 3 -x node ./micro_sorting >> ~/results/orq-3pc.txt

../scripts/run_experiment.sh -p 4 -s lan -c nocopy -n 17 -r 16-20 -T 17 -b 16384 -e 3 -x node -m -DUSE_DALSKOV_FANTASTIC_FOUR=ON ./micro_sorting > ~/results/orq-4pc.txt
cmake .. -DPROTOCOL=4 -DUSE_DALSKOV_FANTASTIC_FOUR=OFF
