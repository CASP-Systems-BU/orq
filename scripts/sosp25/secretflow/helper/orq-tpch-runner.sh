RESULT_DIR="$1"

cd ~/orq/scripts

./query-experiments.sh secretflow 16 2 16 lan | tee orq-sf-queries-16M.txt

mv orq-sf-queries-16M.txt $RESULT_DIR/
