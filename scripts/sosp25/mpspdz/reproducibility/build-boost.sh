sudo apt install -y build-essential g++ python3-dev autotools-dev libicu-dev libbz2-dev
cd /tmp
wget https://downloads.sourceforge.net/project/boost/boost/1.75.0/boost_1_75_0.tar.gz
tar xf boost_1_75_0.tar.gz
cd boost_1_75_0
./bootstrap.sh
./b2 -j$(nproc)
sudo ./b2 install
