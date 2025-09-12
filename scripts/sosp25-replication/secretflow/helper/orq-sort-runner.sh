RESULT_DIR="$1"

cd ~/orq/scripts

# 64bit
./run_experiment.sh -p 2 -s lan -c nocopy -x node -m "-DTRIPLES=ZERO -DDEFAULT_BITWIDTH=64" -e 3 -r 5 -d -T 17 -n 8 -b -1 micro_sorting_rs | tee orq-64b-lan.txt
./run_experiment.sh -p 2 -s lan -c nocopy -x node -m "-DTRIPLES=ZERO -DDEFAULT_BITWIDTH=64" -e 3 -r 6-7 -d -T 17 -n 8 -b -8 micro_sorting_rs | tee -a orq-64b-lan.txt

# 32bit
./run_experiment.sh -p 2 -s lan -c nocopy -x node -m "-DTRIPLES=ZERO -DDEFAULT_BITWIDTH=32" -e 3 -r 5 -d -T 17 -n 8 -b -1 micro_sorting_rs | tee orq-32b-lan.txt
./run_experiment.sh -p 2 -s lan -c nocopy -x node -m "-DTRIPLES=ZERO -DDEFAULT_BITWIDTH=32" -e 3 -r 6-7 -d -T 17 -n 8 -b -8 micro_sorting_rs | tee -a orq-32b-lan.txt


mv orq-64b-lan.txt $RESULT_DIR/
mv orq-32b-lan.txt $RESULT_DIR/
