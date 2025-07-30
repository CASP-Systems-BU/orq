# Install go
mkdir -p ~/src && cd ~/src
wget https://go.dev/dl/go1.23.2.linux-amd64.tar.gz
rm -rf /usr/local/go
sudo tar -C /usr/local -xzf go1.23.2.linux-amd64.tar.gz
export PATH=$PATH:/usr/local/go/bin
echo 'export PATH=$PATH:/usr/local/go/bin' >> ~/.bashrc
cd ~
go version

# Install Docker
sudo apt-get update
sudo apt-get install -y ca-certificates curl
sudo install -m 0755 -d /etc/apt/keyrings
sudo curl -fsSL https://download.docker.com/linux/ubuntu/gpg -o /etc/apt/keyrings/docker.asc
sudo chmod a+r /etc/apt/keyrings/docker.asc
echo \
  "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.asc] https://download.docker.com/linux/ubuntu \
  $(. /etc/os-release && echo "$VERSION_CODENAME") stable" | \
  sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
sudo apt-get update
sudo apt-get install -y docker-ce docker-ce-cli containerd.io docker-buildx-plugin docker-compose-plugin
sudo chmod 777 /var/run/docker.sock
docker run hello-world

# Install python plotting libraries
sudo DEBIAN_FRONTEND=noninteractive apt install -y python3-pip
pip3 install pandas
pip3 install matplotlib

cd ~

# Cloning scql and spu
# git clone --branch vldb2024-fork git@github.com:nitinm25/scql.git
# git clone --branch vldb-tests git@github.com:nitinm25/spu.git
git clone --branch vldb2024-fork https://github.com/nitinm25/scql.git
git clone --branch vldb-tests https://github.com/nitinm25/spu.git

# Copy sort experiment from the scql directory to spu for use inside container
mv ~/scql/vldb/docker-compose/data ~/data-bak  # "data" dir is large, moving it out of the way temporarily
cp -r ~/scql/vldb/ ~/spu/
mv ~/data-bak ~/scql/vldb/docker-compose/data

cd ~/spu

# Run container
docker run -d -it --name spu-dev-$(whoami) --mount type=bind,source="$(pwd)",target=/home/admin/dev/ -w /home/admin/dev --cap-add=SYS_PTRACE --security-opt seccomp=unconfined --cap-add=NET_ADMIN --privileged=true --entrypoint="bash" secretflow/ubuntu-base-ci:20240710

# Setup inside container
docker exec -it spu-dev-$(whoami) bash -c "python3 -m pip install -r requirements.txt"
docker exec -it spu-dev-$(whoami) bash -c "python3 -m pip install -r requirements-dev.txt"
docker exec -it spu-dev-$(whoami) bash -c "apt-get update"
docker exec -it spu-dev-$(whoami) bash -c "apt install iputils-ping iperf3 tmux iproute2 -y"

# Build the two sort scripts
docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk -c opt -- --numel=1000"
docker exec -it spu-dev-$(whoami) bash -c "bazel run //vldb/sort-test:sort_sbk_valid -c opt -- --numel=1000"
