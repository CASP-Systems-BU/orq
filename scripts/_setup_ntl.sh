echo "==== Building NTL..."

wget https://libntl.org/ntl-11.6.0.tar.gz
tar -xvzf ntl-11.6.0.tar.gz
rm *.tar.gz
mv ntl-11.6.0 ntl
cd ntl/src

# prevent illegal instruction issue
case "$(uname -m)" in
    x86_64|amd64|AMD64)
        if [[ -n "${GITHUB_ACTIONS:-}" ]]; then
            ./configure PREFIX=../../ntl-install NTL_GMP_LIP=off NATIVE=off \
                CXXFLAGS="-O2 -march=x86-64-v3"
        else
            ./configure PREFIX=../../ntl-install NTL_GMP_LIP=off
        fi
        ;;
    *)
        ./configure PREFIX=../../ntl-install NTL_GMP_LIP=off
        ;;
esac
make
make install
