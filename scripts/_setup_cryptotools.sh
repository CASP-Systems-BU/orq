# We don't need to _install_ libOTe, but we do need cryptotools. Get it from libOTe so we don't
# have multiple separate installs.
source $(dirname $0)/_clone_libote.sh

cd cryptoTools
python3 build.py --par=4 --install=../../libOTe-install