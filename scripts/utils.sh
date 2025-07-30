#!/bin/bash

define_defaults_and_build(){
    mkdir -p ../build && cd ../build && rm -rf *
    if (( $# > 0 ))
    then
        exp_protocol=${1}
    fi

    if (( $# > 1 ))
    then
        exp_setting=${2}
    fi

    if [ ${exp_protocol} = "semi" ]
    then
        cmake ..
        make clean
        make ${exp_name}
    elif [ ${exp_protocol} = "mal" ]
    then
        cmake .. -DPROTOCOL_VAR='-D PROTCOL_NUM=4'
        make clean
        make ${exp_name}
        cmake ..
    fi

    mkdir -p ../results/${exp_setting}
}