The following instructions describe how to set up [SMCQL](https://github.com/smcql/smcql) on an OpenStack environment.

# Install prerequistes
`sudo apt-get install postgresql postgresql-contrib`   
`sudo apt-get install maven`   
`sudo apt-get install openjdk-8-jdk`   

To choose the default installed `Java` version 8.
`sudo update-alternatives --config java`

# Clone the project code
`cd ~/`   
`git clone https://github.com/smcql/smcql.git`   


# Prepare users
During the following steps a password prompt will appear, it should be put in the file ~/smcql/conf/setup.localhost . Please, keep the password the same for all workers.

`sudo su - postgres`  
`createuser -sPE ubuntu`  
`exit`   

# Setting SSH configuration
- Add the following line to `sudo nano /etc/ssh/sshd_config`.
`KexAlgorithms curve25519-sha256@libssh.org,ecdh-sha2-nistp256,ecdh-sha2-nistp384,ecdh-sha2-nistp521,diffie-hellman-group-exchange-sha256,diffie-hellman-group14-sha1,diffie-hellman-group-exchange-sha1,diffie-hellman-group1-sha1`
- Make sure the `id_rsa` file in readable format by smcql.
`ssh-keygen -p -f /home/ubuntu/.ssh/id_rsa -m pem`
- Restart the `SSH` server.
`sudo service sshd restart`


# Setup Database and compile project
- The data is stored in the directory `conf/workload/testDB`, since there should be max of 2 workers in the system; the directory has two subdirectories called 1 and 2.
- The data can be generated using the following [script](https://github.com/multiparty/conclave/blob/master/benchmarks/aspirin/smcql_gen.sh) from the Conclave repository.
- `cd ~/smcql/`
- `./setup.sh`

# Setup the workers 
- There two files that are important for setup are `conf/setup.localhost` and `conf/connections/localhost`. By default the project is set to use only them for configuration and not the remote files.
- The file called `conf/connections/localhost` should be adjusted to have for example (adjust IPs accordingly):    
"testDB1=ubuntu@10.0.0.11:5432,smcql_testDB_site1,1   
testDB2=ubuntu@10.0.0.4:5432,smcql_testDB_site2,2"
- The file called conf/setup.localhost should be adjusted so that the username is "ubuntu" and the password should be as set above. Password should be common between all users.
- Ensure that the workers have ssh access to each other.
- Update the following file so that it allows postgresql access from remote hosts.   
`sudo nano /etc/postgresql/9.5/main/pg_hba.conf`   
`host  all  all 0.0.0.0/0 md5`   
- Update the following file to accept connections from anyone.
`sudo nano /etc/postgresql/9.5/main/postgresql.conf`
`listen_addresses = '*'`
- Restart PSQL server.
`sudo /etc/init.d/postgresql restart`



