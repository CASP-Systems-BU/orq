cd ~/
mkdir results

sudo apt update
sudo apt upgrade -y
sudo apt install -y automake build-essential clang cmake git libboost-dev libboost-filesystem-dev libboost-iostreams-dev libboost-thread-dev libgmp-dev libntl-dev libsodium-dev libssl-dev libtool python3 python3-pip python3-fabric libstdc++-11-dev

cd ~/orq/scripts/sosp25/mpspdz/reproducibility/
echo "Building Boost 1.75"
./build-boost.sh
cd ~/

# install the correct version of MP-SPDZ
git clone https://github.com/data61/MP-SPDZ.git
cd MP-SPDZ
# commit from December 3rd
git checkout 27220fc954490bdc55516383b7c9ac9eeb33d951

# setup MP-SPDZ
make setup

# copy in the necessary scripts
cp ~/orq/scripts/sosp25/mpspdz/reproducibility/* ~/MP-SPDZ/
mv sort.mpc ~/MP-SPDZ/Programs/Source

##################################################
#                      2PC                       #
##################################################

# setup the 2PC hostfile
rm HOSTS
touch HOSTS
echo node0 >> HOSTS
echo node1 >> HOSTS

# setup the config file for dummy preprocessing
rm CONFIG.mine
touch CONFIG.mine
echo "MY_CFLAGS = -DINSECURE" >> CONFIG.mine

# run 2PC
# make dummy preprocessing and copy it to ~/ and node1:~/
make Fake-Offline.x
./Fake-Offline.x 2
cp -r Player-Data/ ~/
scp -r ~/Player-Data/ node1:~/

./run.sh 2 sort 16 22 > ~/results/2pc-mpspdz.txt

##################################################
#                      3PC                       #
##################################################

echo node2 >> HOSTS
rm CONFIG.mine

./run.sh 3 sort 16 25 > ~/results/3pc-mpspdz.txt

##################################################
#                      4PC                       #
##################################################

echo node3 >> HOSTS

./run.sh 4 sort 16 20 > ~/results/4pc-mpspdz.txt
