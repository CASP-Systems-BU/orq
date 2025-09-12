RESULT_DIR="$1"

# Run experiment
for i in {1..3}; do bash ~/scql/vldb/tpch_scripts/run-log.sh 1 16777216 | tee -a s1-q6-16M.txt; done
for i in {1..3}; do bash ~/scql/vldb/tpch_scripts/run-log.sh 2 16777216 | tee -a s2-q1-16M.txt; done
for i in {1..3}; do bash ~/scql/vldb/tpch_scripts/run-log.sh 3 16777216 | tee -a s3-q14-16M.txt; done
for i in {1..3}; do bash ~/scql/vldb/tpch_scripts/run-log.sh 4 16777216 | tee -a s4-q12-16M.txt; done
for i in {1..3}; do bash ~/scql/vldb/tpch_scripts/run-log.sh 5 16777216 | tee -a s5-q1-mod-16M.txt; done

mv s1-q6-16M.txt $RESULT_DIR/
mv s2-q1-16M.txt $RESULT_DIR/
mv s3-q14-16M.txt $RESULT_DIR/
mv s4-q12-16M.txt $RESULT_DIR/
mv s5-q1-mod-16M.txt $RESULT_DIR/
