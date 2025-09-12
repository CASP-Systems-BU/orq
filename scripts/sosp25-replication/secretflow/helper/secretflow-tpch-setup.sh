# Docker setup
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

# Load image
cd ~/scql_image
docker load -i scql-aws-vldb-image || { echo -e "\nSecretflow docker image not found, exiting setup"; exit 1; }

# Clone repo
cd ~
git clone --branch vldb2024-fork https://github.com/nitinm25/scql.git
cd scql

# Generate data (Commented - Should be a part of the AMI)
# cd vldb/scql-operator-test
# mkdir ../docker-compose/data/
# python3 generate_data.py
# python3 tpch_mock.py -r 20000000

# Bring up containers
cd ~/scql
mv ~/scql/vldb/docker-compose/data ~/data-bak
bash vldb/docker-compose/setup.sh
mv ~/data-bak ~/scql/vldb/docker-compose/data

# NOTE: wait until all containers are running, which may take several minutes to load data.
(cd vldb/docker-compose && docker compose -p vldb down)
(cd vldb/docker-compose && docker compose -p vldb up -d)

sleep 5

# Periodically check if broker containers are up
while docker ps --filter "name=vldb-broker" --format '{{.Status}}' | grep -q "Restarting"; do
  echo "Waiting for broker containers to be ready, this can take several minutes..."
  sleep 10
done

sleep 60

# Setup table settings
bash vldb/tpch_scripts/prepare.sh
