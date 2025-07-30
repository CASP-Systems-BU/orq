./orq.sh
cd ~/orq/scripts/sosp25/mpspdz/reproducibility/
./mpspdz.sh

# generate the plot
cd
home_dir=$(pwd)
echo $home_dir
cd orq/scripts/sosp25/mpspdz/

sudo apt install python3.10-venv -y
python3 -m venv env
. env/bin/activate
pip install matplotlib pandas

python3 plot_mpspdz_sort.py $home_dir
mv mpspdz-compare.png ~/results/
