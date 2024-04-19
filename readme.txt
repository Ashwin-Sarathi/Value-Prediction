Instructions to build the 721sim executable:
    1. cd 721/Value-Prediction/build
    2. cmake -DCMAKE_BUILD_TYPE=RelO3 ../721sim / cmake -DCMAKE_BUILD_TYPE=debug ../721sim
    3. make -j$(nproc) 
The executable will be located at the path: 721/P4/Value-Prediction/uarchsim/721sim
Can do ./uarchsim/721sim -h for help

Instructions to run the simulator:
    1. Enter run directory, then enter the directory bearing the desrired benchmark and parameters:
        mkdir 473.astar_rivers_ref.252.0.28.gz-2
        cd 473.astar_rivers_ref.252.0.28.gz-2
    2. Activate tool for setting up run directories: 
        source /mnt/designkits/spec_2006_2017/O2_fno_bbreorder/activate.bash
    3. Symbolically link to the proxy kernel: 
        ln -s /mnt/designkits/spec_2006_2017/O2_fno_bbreorder/app_storage/pk
    4. Generate makefile for the desired checkpoint: 
        atool-simenv mkgen 473.astar_rivers_ref --checkpoint 473.astar_rivers_ref.252.0.28.gz
    5. Symbolically link to 721sim:
        ln -s ../../build/uarchsim/721sim 721sim
    6. Launch a 721sim job (change flags as necessary): 
        make cleanrun SIM_FLAGS_EXTRA='--mdp=4,0 --perf=0,0,0,0 --fq=64 --cp=32 --al=256 --lsq=128 --iq=64 --iqnp=4 --fw=4 --dw=4 --iw=8 --rw=4 -e10000000'