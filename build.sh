#!/bin/bash

WS=$(cd $(dirname $0);pwd)

BOLD='\033[1m'
RED='\033[0;31m'
GREEN='\033[32m'
WHITE='\033[34m'
YELLOW='\033[33m'
NO_COLOR='\033[0m'
BLUE='\033[0;34m'

function info() {
    (>&2 echo -e "[${WHITE}${BOLD} INFO ${NO_COLOR}] $*")
}

function error() {
    (>&2 echo -e "[${RED} ERROR ${NO_COLOR}] $*")
}

function warning() {
    (>&2 echo -e "[${YELLOW} WARNING ${NO_COLOR}] $*")
}

function ok() {
    (>&2 echo -e "[${GREEN}${BOLD} OK ${NO_COLOR}] $*")
}

function print_delim() {
    echo '=================================================='
}

function get_now() {
    echo $(date +%s)
}

function print_time() {
    END_TIME=$(get_now)
    ELAPSED_TIME=$(echo "$END_TIME - $START_TIME" | bc -l)
    MESSAGE="Took ${ELAPSED_TIME} seconds"
    info "${MESSAGE}"
}

function success() {
    print_delim
    ok "$1"
    print_time
    print_delim
}

function fail() {
    print_delim
    error "$1"
    print_time
    print_delim
}

function print_usage() {
    echo -e "\n${RED}Usage${NO_COLOR}:
    .${BOLD}/build.sh${NO_COLOR} [OPTION]"

    echo -e "\n${RED}Options${NO_COLOR}:
    ${BLUE}all${NO_COLOR}: run all
    ${BLUE}build${NO_COLOR}: run the code build
    ${BLUE}clean${NO_COLOR}: clean the code build
    ${BLUE}cov${NO_COLOR}: run the code test coverage
    ${BLUE}check_code${NO_COLOR}: run the code style check
    "
}

function clean() {
    rm -rf build build_dist
}

function get_dependencies() {
    echo "get-dependencies"
    mkdir -p build/modules
    wget http://10.10.173.10/share/pkgs/get-dependencies.py
    python get-dependencies.py
    rm -rf get-dependencies.py
}

function check_code() {
    wget http://10.10.173.10/release/code_check_tools/code_check_tools.tar.gz
    tar -zxvf code_check_tools.tar.gz
    chmod -R +x code_check_tools
    export WORKSPACE=${WS}
    export CODE_CHECK_EXCLUDE_LIST="can,memory_check,communication,build_dist,build"
    ./code_check_tools/code_check.sh run
    rm -rf code_check_tools.tar.gz code_check_tools
}

function build_make() {
    MAX_CPU_NUM=$(nproc)
    cd ${WS}/build
    make -j$[${MAX_CPU_NUM}-1] install

    if [ $? -eq 0 ]; then
        success 'Build passed!'
        exit 0
    else
        fail 'Build failed!'
        exit 1
    fi
}

function build() {
    mkdir -p ${WS}/build && cd ${WS}/build
    cmake -DWITH_COV=${WITH_COV} \
          -DDO_TEST=${DO_TEST} \
          ${EXTRA_OPTIONS} \
          -DCMAKE_INSTALL_PREFIX=${WS}/build_dist/crdc_airi_common ..
    build_make
}

function main() {
    local cmd=$1
    cd ${WS}
    if [ -z $1 ]; then
        if [ ! -d "build" ]; then
            cmd=all
        else
            cmd=build
        fi
    fi
    shift

    START_TIME=$(get_now)
    BUILD_DIST=OFF
    WITH_COV=OFF
    DO_TEST=OFF
    # get_dependencies
    if [[ "${PLATFORM}" == "TDA4" ]];then
        source $(find /opt/ -name environment-setup-aarch64-linux)
    elif [[ "${PLATFORM}" == "A6" ]];then
        source /opt/hirain-imx-linux/4.14-sumo/environment-setup-aarch64-poky-linux
    fi

    if [ "${ARCH}" = "arm64" ]; then
        EXTRA_OPTIONS="${EXTRA_OPTIONS} -DWITH_AARCH64=ON"
    fi
    case $cmd in
        build)
            build_make
            ;;
        all)
            build
            ;;
        clean)
            clean
            ;; 
        cov)
            WITH_COV=ON
            DO_TEST=ON
            build
            ;;
        check_code)
            check_code
            ;;
        *)
            print_usage
            ;;
    esac
}

main $@