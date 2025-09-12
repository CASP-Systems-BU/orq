# generate the plot
cd
home_dir=$(pwd)
echo $home_dir
cd orq/scripts/sosp25/sorting-main/

sudo apt install python3.10-venv -y
python3 -m venv env
. env/bin/activate
pip install matplotlib pandas

python3 plot_sorting.py $home_dir $1
mv plot-sort.png ~/results/sorting-main/sorting-main.png