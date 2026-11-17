# We don't need to _install_ libOTe, but we do need cryptotools. Get it from libOTe so we don't
# have multiple separate installs.
source $(dirname $0)/_clone_libote.sh

cd cryptoTools

# prevent illegal instruction issue
case "$(uname -m)" in
    x86_64|amd64|AMD64)
        ISA_FLAGS=
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            ISA_FLAGS="-DENABLE_AVX512=OFF"
        fi
        ;;
    *)
        ISA_FLAGS="-DENABLE_SSE=OFF -DENABLE_AVX=OFF -DENABLE_AVX512=OFF"
        ;;
esac
python3 build.py --par=4 --install=../../libOTe-install $ISA_FLAGS