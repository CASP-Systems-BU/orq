sudo apt update
sudo apt upgrade -y

# all other dependencies are installed by the secure-join build script
sudo apt install cmake libtool -y

# assumes the library is already pulled from github
cd secure-join
python build.py

# copy the frontend file to the home directory
cp ~/secure-join/out/build/linux/frontend/secJoinfrontend ~/
