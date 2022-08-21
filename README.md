## Description

* The project include all the common codes and some usefull scritps.

## Build

* Set env for different platform
`export PLATFORM=X86`  `export TAG=1604`  `export TAG=1804` for X86
`export PLATFORM=TDA4` `export TAG=0702`  `export TAG=0703` `export TAG=0703_HI` for TDA4
`export PLATFORM=A6` `export TAG=""` for A6

`./build.sh` build the project.
`./build.sh clean` clean the build.
run `./build.sh help` to see the details.


## Release
* If the build successed, all the release are in the `build_dist` folder.

### help menu
./build.sh help

```
Usage:
    ./build.sh [OPTION]

Options:
    all: run all
    build: run the code build
    clean: clean the code build
    cov: run the code test coverage
    check_code: run the code style check
```

## Module

### common
1. singleton
2. thread
3. factory
4. concurrent_queue
5. concurrent_object_pool

### framework
* The framework is used to create application.
* If the build successed. use `execute.sh` in the `build_dist/build_dist/crdc_airi_common/bin/` to execute the app the default config is param `build_dist/crdc_airi_common/params/framework/production/dag_streaming.prototxt`
* `tools/vis_perception_dag.py` could be used to generate `DAG picture`.
* ![dag](modules/framework/tools/test.png). 
* Use command `python vis_perception_dag.py -i [input_dag_file_path] -o [output_picture_file_path]` in the `ubuntu:18.04-hirain-inner`.