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
DEFAULT_MAX_SIZE=29
MAX_SIZE=${1:-$DEFAULT_MAX_SIZE}
echo "Max power of two: $MAX_SIZE"

# run cmake once to set the triples to zeroes
cmake .. -DPROTOCOL=2 -DTRIPLES=ZERO

../scripts/run_experiment.sh -p 2 -s lan -c nocopy -n 8 -r 20-$MAX_SIZE -T 34 -b -12 -e 3 -x node ./micro_sorting >> ~/results/sorting-main/2pc.txt
../scripts/run_experiment.sh -p 3 -s lan -c nocopy -n 8 -r 20-$MAX_SIZE -T 34 -b -12 -e 3 -x node ./micro_sorting >> ~/results/sorting-main/3pc.txt
../scripts/run_experiment.sh -p 4 -s lan -c nocopy -n 8 -r 20-$MAX_SIZE -T 34 -b -12 -e 3 -x node ./micro_sorting >> ~/results/sorting-main/4pc.txt

# generate the plot
cd
home_dir=$(pwd)
echo $home_dir
cd orq/scripts/sosp25/sorting-main/

sudo apt install python3.12-venv -y
python -m venv env
. env/bin/activate
pip install matplotlib pandas

python plot_sorting.py $home_dir $MAX_SIZE
mv plot-sort.png ~/results/sorting-main/sorting-main.png

