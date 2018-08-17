#!/bin/bash
CXX=g++
CXXFLAGS="-g -O2 -march=native -pipe -fomit-frame-pointer -Wno-deprecated -DNDEBUG -Wall"
LDFLAGS=

#You can remove the module you do not need, even make MODULE_LIST empty
MODULE_LIST=(net stackful time)
compile_files=""
for module_name in ${MODULE_LIST[@]}
do	
    compile_files="$compile_files $module_name/*.cpp"	 
	if(assembile_files=$(ls $module_name/*.S 2>/dev/null)); then
		compile_files="$compile_files $module_name/*.S"	 
	fi
done

rm -f *.o
#You can add your own option when execute "./make_lib.sh" , for example, "./make_lib.sh  -fPIC"
input_options=$@
set -x
$CXX $CXXFLAGS $LDFLAGS $input_options $compile_files -c 
ar rcs libcort_proto.a *.o