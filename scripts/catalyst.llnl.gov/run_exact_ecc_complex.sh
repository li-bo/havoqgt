#!/usr/bin/env bash

# ---------------------------------------------------------------------------------------------------- #
# Command line options
# ---------------------------------------------------------------------------------------------------- #
graph_name=$1
num_nodes=$2
k=$3
tag=$4
time_limit=$5

# ---------------------------------------------------------------------------------------------------- #
# Configurations
# ---------------------------------------------------------------------------------------------------- #f
storage_path="/p/lscratchh/iwabuchi/"
procs=36

# ---------------------------------------------------------------------------------------------------- #
# Functions
# ---------------------------------------------------------------------------------------------------- #
function make_script
{
    rm -f ${sbatch_out}
    echo "#!/bin/bash" > ${sh_file}
    echo "#SBATCH -N${num_nodes}" >> ${sh_file}
    echo "#SBATCH -o ${sbatch_out}" >> ${sh_file}
    echo "#SBATCH --ntasks-per-node=${procs}" >> ${sh_file}
    echo "#SBATCH -t ${time_limit}" >> ${sh_file}

    echo "${option}" >> ${sh_file}

    echo "srun --ntasks-per-node=${procs} --distribution=block ./src/transfer_graph ${graph_path} /dev/shm/" >> ${sh_file}

#    echo "export HAVOQGT_MAILBOX_SHM_SIZE=16384" >> ${sh_file}
#    echo "export HAVOQGT_MAILBOX_MPI_SIZE=131072" >> ${sh_file}
    echo "srun --drop-caches=pagecache -N${num_nodes} --ntasks-per-node=${procs} --distribution=block ./src/${exe_file_name} -i /dev/shm/graph  -e ${ecc_file_name} ${source_selection_algorithms} -c ${two_core_file_name}" >> ${sh_file}
}

function compile()
{
        num_sources=$1
        export NUM_SOURCES=${num_sources}
        sh scripts/do_cmake.sh
        make transfer_graph run_exact_ecc #ingest_edge_list
        cp ./src/run_exact_ecc ./src/${exe_file_name}
}

# ---------------------------------------------------------------------------------------------------- #
# Main
# ---------------------------------------------------------------------------------------------------- #
base_graph_path="${storage_path}/${graph_name}"
ecc_out_path="${storage_path}/${graph_name}/ecc/"

unset USE_TAKE
unset USE_TAKE_PRUN
unset USE_NEW_ADP
unset USE_NEW_MAX_U

if [ "${tag}" == "tk" ]; then
    option="export USE_TAKE=1" ## 1 is a dummy value
    source_selection_algorithms=""
elif [ "${tag}" == "adp_new" ]; then
    option="export USE_NEW_ADP=10"
    source_selection_algorithms="-a 0:1:2:3:4:5:6:7"
elif [ "${tag}" == "adp_new_maxu" ]; then
    option="export USE_NEW_ADP=10"
    option="export USE_NEW_MAX_U=1"
    source_selection_algorithms="-a 0:1:2:3:4:5:6:7"
elif [ "${tag}" == "adp_new_tk_pr" ]; then
    option="export USE_TAKE_PRUN=1; export USE_NEW_ADP=10"
    source_selection_algorithms="-a 0:1:2:3:4:5:6:7"
fi

sbatch_out="sbatch_${graph_name}_n${num_nodes}_k${k}_${tag}.out"
sh_file="run_${graph_name}_n${num_nodes}_k${k}_${tag}.sh"
exe_file_name="run_exact_ecc_k${k}_${tag}"
ecc_file_name="${ecc_out_path}/ecc_k${k}_${tag}"
two_core_file_name="${base_graph_path}/2core_table"
graph_path="${base_graph_path}/n${num_nodes}/graph"

make_script
compile ${k}

sbatch ${sh_file}
