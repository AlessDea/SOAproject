cmake_minimum_required(VERSION 3.23)
project(SOAproject C)

set(CMAKE_C_STANDARD 11)



include_directories(rcu_list)
include_directories(src)
include_directories(usctm/include)

add_executable(SOAproject
        src/list.h
        rcu_list/rcu-list.c
        src/dir.c
        src/file.c
        src/helper.h
        src/singlefilefs.h
        src/singlefilefs_src.c
        src/singlefilemakefs.c
        usctm/include/vtpmo.h
        usctm/lib/vtpmo.c
        usctm/the_usctm.mod.c
        usctm/usctm.c)

