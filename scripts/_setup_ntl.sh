echo "==== Building NTL..."

wget https://libntl.org/ntl-11.6.0.tar.gz
tar -xvzf ntl-11.6.0.tar.gz
rm *.tar.gz
mv ntl-11.6.0 ntl
cd ntl/src

./configure PREFIX=../../ntl-install NTL_GMP_LIP=off
make
make install
