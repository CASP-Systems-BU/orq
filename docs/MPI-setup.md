#  Setting up MPI on OpenStack
The following instructions assume 3 Ubuntu VMs with established SSH connectivity.

## Install Prerequisites:
`sudo apt install build-essential`  
`sudo apt-get install manpages-dev`  
`sudo apt-get install gfortran`  

## Setup MPI on all nodes:
- Download and extract installation files:  
`wget https://www.mpich.org/static/downloads/1.4/mpich2-1.4.tar.gz`  
`tar -xzf mpich2-1.4.tar.gz`  
`cd mpich2-1.4`  
- Install MPI  
`./configure`  
`make; sudo make install`  

## MPI configuration on the broker node:
- Install nfs that is used by MPI to share files:  
`sudo apt-get install nfs-kernel-server`
- Create a folder `Secrecy` and clone the framework code.
- Share the project directory with other nodes:  
`sudo nano /etc/exports`  
and add the following line in the file  
`/home/ubuntu/Secrecy *(rw,sync,no_root_squash,no_subtree_check)`  
- Apply changes:  
`sudo exportfs -a`  
`sudo service nfs-kernel-server restart`  

## MPI configuration on other nodes.
`sudo apt-get install nfs-common`  

- Create a folder `Secrecy` and clone the framework code.  

- Mount the shared Secrecy directory (Note: "ubuntu-16-lts-test-two" represents the name of the broker node)  
`sudo mount -t nfs ubuntu-16-lts-test-two:/home/ubuntu/Secrecy ~/Secrecy`  
`df -h`  

and open the following file:  
`sudo nano /etc/fstab`  
to add this line:  
`ubuntu-16-lts-test-two:/home/ubuntu/Secrecy /home/ubuntu/Secrecy nfs`  


## Running the framework:
- Open the code home directory:  
`cd ~/Secrecy`  
- Set the computing nodes by creating a host_file. This file will have the names of the computing parties.
`nano host_file`  
- Add the following lines in the host_file:  
`ubuntu-16-lts-test-one:1`  
`ubuntu-16-lts-test-two:1`  
`ubuntu-16-lts-test-three:1`  

- Run a query!
