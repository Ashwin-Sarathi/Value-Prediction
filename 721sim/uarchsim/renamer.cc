#include "renamer.h"
#include <algorithm>
#include <cassert>
#include <iostream>
// #include "pipeline.h"

using namespace std; 

//-------------------------------------------------------------------
//Constructor Function
//-------------------------------------------------------------------

renamer:: renamer(uint64_t n_log_regs, uint64_t n_phys_regs, uint64_t n_branches, uint64_t n_active)
        : rmt_amt_size(n_log_regs), prf_size(n_phys_regs), smt_size(n_log_regs), total_checkpoints(n_branches){


        // Assertions as described in the preconditions 
        assert(n_phys_regs > n_log_regs);
        assert(n_branches >= 1 && n_branches <= 64);
        assert(n_active > 0);

        // Allocate and initialize the RMT, AMT, PRF, PRF ready bit array
        rename_map_table = new uint64_t[n_log_regs];
        architectural_map_table = new uint64_t[n_log_regs];
        physical_register_file  = new uint64_t[n_phys_regs];
        prf_ready_bit = new uint64_t[n_phys_regs];
        
        //Initializing the RMT and AMT
        //In the beginning the RMT and AMT will hold the same values until the AMT is changed with a committed value from the active list
        uint64_t backward_index = n_log_regs -1; 
        for (uint64_t i = 0; i < n_log_regs; ++i) {
            architectural_map_table[backward_index -i] = rename_map_table[backward_index -i] = i;
        }

        //PRF bits needs to be set to 1 to mark all of the instructions as complete
        //fill(prf_ready_bit, prf_ready_bit + n_phys_regs, 1); 
        for(uint64_t i = 0; i < n_phys_regs; ++i){
            prf_ready_bit[i] = 1; 
        }

        //Intialiazing the Active list 
        activeList.list = new ActiveListEntry[n_active];
        activeList.size = n_active;         //Active List size >= Free List size   
        activeList.head = 0; 
        activeList.tail = 0; 
        activeList.head_phase_bit = 0;  //During the inital state, the head is intitalzied to 0 and tail is initalized to 0
        activeList.tail_phase_bit = 0; 

        //Intialize the active list entry
        for(uint64_t k = 0; k < n_active; ++k){
            activelist_initalize(&activeList.list[k]); 
        }

        //Intialiazing the Free list 
        freelist.head = 0; 
        freelist.tail = 0; 
        freelist.head_phase_bit = 0; //During the inital state, the head is intitalzied to 0 and tail is initalized to 1
        freelist.tail_phase_bit = 1;
        freelist.size = n_phys_regs - n_log_regs;    //Free List size (max # free registers for speculation) = PRF size - # logical registers (committed state can’t be free) 
        freelist.list = new uint64_t[freelist.size];  

        //Initialzie the freelist with registers not present in the RMT and AMT 
        //If logical registers = 32, RMT and AMT initialzied from r0-r31
        //free list will be initialzied from r32-r? Depending on the prf size
        for (uint64_t i = 0; i < freelist.size; ++i) {
            freelist.list[i] = i + n_log_regs; 
        }

        // Allocate and initialize the Branch Checkpoints
        GBM = 0;
        branchCheckpoint = new BranchCheckpoint[total_checkpoints]; 
    }

    // Destructor
    renamer:: ~renamer() {
        delete[] rename_map_table; 
        delete[] architectural_map_table; 
        delete[] physical_register_file; 
        delete[]prf_ready_bit; 
        //delete[]activeList;
        //delete[] branchCheckpoints; 
    }


//-------------------------------------------------------------------
//Rename Stage
//-------------------------------------------------------------------


    bool renamer:: stall_reg(uint64_t bundle_dst){
        //have to access the free list contents to get the free physical registers available 
        uint64_t free_physical_registers; 
        if(freelist_full()){
            free_physical_registers = freelist.size;
        }
        else if(freelist_empty()){
            free_physical_registers = 0; 
        }
        //Normal state
        else if (freelist.tail_phase_bit == freelist.head_phase_bit){
            free_physical_registers =  freelist.tail - freelist.head; 
        }
        //if the free list has wrapped around  
        else if (freelist.tail_phase_bit != freelist.head_phase_bit){
            free_physical_registers = freelist.tail - freelist.head + freelist.size; //Between head and tail: list of free physical registers (//721SS-PRF-2 (slide 18))
        } 
        else {
            free_physical_registers = UINT64_MAX;
            cerr << "Free List Managed Incorrectly" << endl;
        }
        
        //Check if the available registers can handle the bundle 
        if(free_physical_registers < bundle_dst){    //Return "true" (stall) if there aren't enough free physical registers that are enough for the instruction bundle 
            return true;
        }
        return false; 
    }

    bool renamer::stall_branch(uint64_t bundle_branch) {

        uint64_t free_checkpoints = 0;
        uint64_t temp_gbm = GBM;

        // If all of the bits are set in GBM, then all checkpoints are taken and hence must stall
        if (temp_gbm == UINT64_MAX) return true;

        uint64_t mask = 1;
        // Loop through each bit in GBM to count the number of free checkpoints
        for (uint64_t i = 0; i < total_checkpoints; ++i) {
            if (!(temp_gbm & mask)) { // If the bit is not set, the checkpoint is free
                free_checkpoints++;
            }
            mask <<= 1; // Move to the next bit
        }

        // Stall (return true) if not enough free checkpoints for the branches
        return bundle_branch > free_checkpoints;
    }

	uint64_t renamer:: get_branch_mask(){
        return GBM;                       //gets the branch mask for an instruction
    }


	uint64_t renamer:: rename_rsrc(uint64_t log_reg){
        //rename logical source registers: obtain mappings from rename map table
        return rename_map_table[log_reg]; 
    }


	uint64_t renamer:: rename_rdst(uint64_t log_reg){

        // 1. Pop a free physical register from the free list
        uint64_t physical_register;


        // 1a. Determine if the free list is empty. If not empty, the processor needs to stall
        if (freelist_empty()) {
           physical_register = UINT64_MAX; // Indicates an error condition
            cerr << "Error: No free physical registers available." << endl;
            return UINT64_MAX; 
        }

        //2a. Get the physical register from the head of the free list 
        physical_register = freelist.list[freelist.head];

        //3a. Must update the free list head
        freelist.head++; 
        //Head pointer & head phase bit (if the head wraps around the list again)
        if(freelist.head == freelist.size){
            freelist.head_phase_bit = !freelist.head_phase_bit;
            freelist.head =0; 
        }
   
        // if (physical_register == UINT64_MAX) { // Check for error condition
        //     cerr << "Error: No free physical registers available." << endl;
        //     return UINT64_MAX; 
        // }

        //if a physical register has been successfully gotten from the free list
        // 2. Assign the physical register to the logical destination register
        // 3. Update rename table to reflect the new mappings
            update_rename_map_table(log_reg, physical_register);

        //4. Checking AMT: Used to debug to make sure AMT is properly managed 
        for(uint64_t i = 0; i< rmt_amt_size; ++i){
            if(architectural_map_table[i] == physical_register){
              cerr << "Error: The free regsister is already in the AMT." << endl;  
            }
        }

        return physical_register; 
    }


    uint64_t renamer:: checkpoint(uint64_t vpq_tail, bool vpq_tail_phase_bit){

        //1.Find a free bit and set the bit to 1
        uint64_t temp_gbm = GBM; 
        uint64_t temp_index = UINT64_MAX;

        if(temp_gbm == UINT64_MAX){
            temp_index = -1; 
        }

        uint64_t mask = 1; 
        for(uint64_t i = 0; i < total_checkpoints; ++i){
            if(temp_gbm & mask){
                mask <<=1;
            } else {
               temp_index = i;  
               //exit out of the else and for loop
               break; 
            }
        }

        GBM |= (1ULL<<temp_index);


        //2. Updating the shadow map table
        store_checkpoint(temp_index, vpq_tail, vpq_tail_phase_bit);

        return temp_index; 
    }



//-------------------------------------------------------------------
//Dispatch Stage
//-------------------------------------------------------------------

    bool renamer:: stall_dispatch(uint64_t bundle_inst){

        //1. Get the number of free entries in the active list 
        uint64_t free_entries;

        if(activelist_full()){
            return true;
        }
        else if(activelist_empty()){
            free_entries = activeList.size;
        }
        else if(activeList.head_phase_bit == activeList.tail_phase_bit){            //tail is equal to or ahead of the head 
            free_entries = activeList.size - (activeList.tail - activeList.head);
        }
        else if(activeList.head_phase_bit != activeList.tail_phase_bit){            //head is ahead of the tail 
            free_entries = activeList.head - activeList.tail;
        }
        else {
            free_entries = -1;      //if the active list was properly managed, it would reach this and stalls 
        }

        // 2. Compare the free entries and bundle of instructions to see if there is enough space 
        if ((free_entries < bundle_inst) || (free_entries <= 0)){
            return true;
        }

        return false;
    }


    uint64_t renamer:: dispatch_inst(bool dest_valid,
                            uint64_t log_reg,
                            uint64_t phys_reg,
                            bool load,
                            bool store,
                            bool branch,
                            bool amo,
                            bool csr,
                            uint64_t PC){


    //1. Check if the active list is full and assert it 
    assert(!activelist_full());
 
    //2. Find a point in the active list to insert the instruction --> at the tail of the list 
    uint64_t new_index; 
    if(activelist_full()){
        new_index = UINT64_MAX; 
        cerr << "Error: Active list is full and cannot insert new instruction." << endl;  
    } 
    else{
        new_index = activeList.tail;
    }

    //3. Increment the tail point
    activeList.tail++; 
    if (activeList.tail == activeList.size) {                                 // If the tail wraps around to the start of the list, change the phase bit
        activeList.tail_phase_bit = !activeList.tail_phase_bit;
        activeList.tail = 0; 
    }

    //4. Fill the new index with the register information 
    activeList.list[new_index].dest_existence = dest_valid;
    if (dest_valid) {   //if it has a destination register, set logical and physical register 
        activeList.list[new_index].logical_dest = log_reg;
        activeList.list[new_index].physical_dest = phys_reg;
    }else {
        activeList.list[new_index].logical_dest = UINT64_MAX;       //the inputs log_reg and phys_reg are ignored 
        activeList.list[new_index].physical_dest = UINT64_MAX;     
    }
   
    //5. Update the active list entry with the provided information 
    activeList.list[new_index].load = load;
    activeList.list[new_index].store = store;
    activeList.list[new_index].branch = branch;
    activeList.list[new_index].amo = amo;
    activeList.list[new_index].csr = csr;
    activeList.list[new_index].pc = PC;

    //6. Set flags to false since the instruction is at its intial stage 
    activeList.list[new_index].completed = false; 
    activeList.list[new_index].exception = false; 
    activeList.list[new_index].load_violation = false; 
    activeList.list[new_index].branch_mispredict = false; 
    activeList.list[new_index].value_mispredict = false; 
    
    //return the instructions index in the active list tail 
    return new_index; 
    }


    bool renamer::is_ready(uint64_t phys_reg){
        if(prf_ready_bit[phys_reg] == 1) {
            return true;
        }
        return false;           
    }

    void renamer::clear_ready(uint64_t phys_reg){
        prf_ready_bit[phys_reg] = 0;
    }


//-------------------------------------------------------------------
//Register Read and Execute Stage
//-------------------------------------------------------------------

    uint64_t renamer::read(uint64_t phys_reg){
        return physical_register_file[phys_reg];
    }

    void renamer::set_ready(uint64_t phys_reg){
        prf_ready_bit[phys_reg] = 1;
    }


//-------------------------------------------------------------------
//Writeback Stage
//-------------------------------------------------------------------

	void renamer:: write(uint64_t phys_reg, uint64_t value){
        physical_register_file[phys_reg] = value;
    }

	void renamer:: set_complete(uint64_t AL_index){
        activeList.list[AL_index].completed = true;
    }

	void renamer:: resolve(uint64_t AL_index,
		     uint64_t branch_ID,
		     bool correct){

    //1. Correct Branch prediction 
        if (correct){ 
    //2. clear the branch; bit in the GBM
            GBM &= ~(1ULL << branch_ID);
    //3. clear all the checkpointed GBMs;
            for (uint64_t i=0; i < total_checkpoints; ++i){
                branchCheckpoint[i].checkpointedGBM &= ~(1ULL<<branch_ID);
            } 
        }
    
    //4. Incorrect Branch Prediction
        else {

    //5. Restore the GBM from the checkpoint: the bit must be cleared in the GBM for the restored branch
            GBM = branchCheckpoint[branch_ID].checkpointedGBM & ~(1ULL << branch_ID); 

    //6. The RMT must be restored from the checkpoint
            for (uint64_t i = 0; i < smt_size; ++i) {
                rename_map_table[i] = branchCheckpoint[branch_ID].shadowMapTable[i];
            }

    //7. The free list head pointer and phase bit must be restored using the branch's checkpoint
            freelist.head = branchCheckpoint[branch_ID].head;
            freelist.head_phase_bit = branchCheckpoint[branch_ID].head_phase_bit;

    //8. The Active list tail pointer and phase bit needs to be restored
            activeList.tail = AL_index + 1;       
        //condiitonal checking to set the phase bit appropriately according to the position of the head and tail pointer 
            if(activeList.tail == activeList.size){
                activeList.tail = 0; 
            }
            if (activeList.head >= activeList.tail) {
            // If head is ahead of tail (wrapped around), toggle the tail phase bit
                activeList.tail_phase_bit = !activeList.head_phase_bit;
            } else if (activeList.head < activeList.tail){
            // If head is behind or at the same position as tail, keep the same phase bit
                activeList.tail_phase_bit = activeList.head_phase_bit;
            }
        }
    }

//-------------------------------------------------------------------
//Retire Stage
//-------------------------------------------------------------------

	bool renamer :: precommit(bool &completed,
                       bool &exception, bool &load_viol, bool &br_misp, bool &val_misp,
	               bool &load, bool &store, bool &branch, bool &amo, bool &csr,
		       uint64_t &PC){

    //1. Check if the Active List is empty
        if (activelist_empty()) {
            return false; 
        }

    //2. Get the instruction at the head of the Active List
        ActiveListEntry &head_entry = activeList.list[activeList.head];

    //3. Get all of the active list entries information from the head entry
        completed = head_entry.completed;
        exception = head_entry.exception;
        load_viol = head_entry.load_violation;
        br_misp = head_entry.branch_mispredict;
        val_misp = head_entry.value_mispredict;
        load = head_entry.load;
        store = head_entry.store;
        branch = head_entry.branch;
        amo = head_entry.amo;
        csr = head_entry.csr;
        PC = head_entry.pc;

        return true; // Instruction present at the head of the active list          
    }

    void renamer :: commit(){

        //1. Assertions before committing the head instruction 
        //Assert if the active list isnt empty
        assert(!activelist_empty()); 
        //Assert if the head instruction is complete
        assert(activeList.list[activeList.head].completed);
        //Assert if it is not marked as an exception 
        assert(!activeList.list[activeList.head].exception);
        //Assert if it is not marked as a load violation 
        assert(!activeList.list[activeList.head].load_violation);

        //2. The AMT needs to be updated if the head instruction of the active list has a destination register 
        //New physical register mapping needs to be added to the AMT
         ActiveListEntry& activelist_head = activeList.list[activeList.head];
         uint64_t prev_register = architectural_map_table[activelist_head.logical_dest];  
        //Only instructions with a valid destination should be pushed into AMT and free previous mappings to the freelist 
        if(activelist_head.dest_existence) {
            assert(!freelist_full());           //for debugging purposes: Currently getting no physical registers available from the freelist. Testing free list management
            architectural_map_table[activelist_head.logical_dest] = activelist_head.physical_dest; 

        //The old physical register from the AMT needs to be freed and added to the free list: 721SS-PRF-2 (slide 16)
        if(freelist_full()){
            cerr << "ERROR: Tried to add when the free list is full\n";
            exit(EXIT_FAILURE);
        } else {
            freelist.list[freelist.tail] = prev_register; 
            freelist.tail++; 
            //handle wrap around case
            if(freelist.tail == freelist.size){
                freelist.tail_phase_bit = !freelist.tail_phase_bit; 
                freelist.tail = 0; 
            }

            //Adding more assertions here to debug free list management 
            // if(freelist.head_phase_bit == freelist.tail_phase_bit){         //free list is in a non wrapped state (partially full or empty). In this state head should be less than tail 
            //     assert(freelist.head <= freelist.tail);
            // }
            // else if (freelist.head_phase_bit != freelist.tail_phase_bit){         //free list is in a wrapped state --> tail should have moved past the head 
            //     assert(freelist.head > freelist.tail);
            // }
            }
        }

        //3. Active list needs to be updated: head needs to move forward: popped from the active list 
        if(activelist_empty()){
            cerr << "ERROR: Tried to retire instruction from active list but is empty\n";
            exit(EXIT_FAILURE);
        } else {
            activeList.head++; 
            //handle wrap around case
            if(activeList.head == activeList.size){
                activeList.head_phase_bit = !activeList.head_phase_bit; 
                activeList.head = 0; 
            }
        }
    }

    void renamer :: squash(){

        //1. AMT needs to be copied to the RMT 
        //AMT contains the committed version of all of the registers and this needs to be propagated to the RMT to restore it
        for(uint64_t i = 0; i < rmt_amt_size; ++i){
            rename_map_table[i] = architectural_map_table[i]; 
        }

        //2. Active list needs to be restored 
        // Active list entry needs to be restored to the initial state (cleared) 
        for(uint64_t j = 0; j < activeList.size; ++j){
            activelist_initalize(&activeList.list[j]); 
        }
        //the inital tail pointer and phase bit needs to be set equal to the head 
        activeList.tail = activeList.head; 
        activeList.tail_phase_bit = activeList.head_phase_bit; 

        //3. Branch Prediction checkpoints need to be reset to make them ready for new allocations 
        for(uint64_t k =0; k < total_checkpoints; ++k){
            checkpoint_initalize(&branchCheckpoint[k]);
        }

        //4. Clear the GBM
        GBM = 0; 

        //5. The free rest needs to be restored 
        freelist.head = freelist.tail; 
        freelist.head_phase_bit = !freelist.tail_phase_bit; 
    }

//-------------------------------------------------------------------
//General Functions
//-------------------------------------------------------------------
    void renamer:: set_exception(uint64_t AL_index){
        activeList.list[AL_index].exception = true;
    }

	void renamer:: set_load_violation(uint64_t AL_index){
        activeList.list[AL_index].load_violation = true;
    }

	void renamer:: set_branch_misprediction(uint64_t AL_index){
        activeList.list[AL_index].branch_mispredict = true;
    }

	void renamer:: set_value_misprediction(uint64_t AL_index){
        activeList.list[AL_index].value_mispredict = true;
    }

    bool renamer:: get_exception(uint64_t AL_index){
        return activeList.list[AL_index].exception;
    }


//-------------------------------------------------------------------
//Private Functions
//-------------------------------------------------------------------

	void renamer:: update_rename_map_table(uint64_t logical_register, uint64_t physical_register){   //updates the mapping of a logical to a physical register in the rename map table
        
        //1. Check if the register from the free list is in the RMT
        for (uint64_t i = 0; i < rmt_amt_size; ++i) {
            if (rename_map_table[i] == physical_register) {
                cerr << "Error: Physical register already in use in RMT." << std::endl;
                return;
            }
        }
        //2. Assign to RMT if the register does not exist in the RMT 
        rename_map_table[logical_register] = physical_register;     
    } 

    bool renamer:: freelist_empty(){  //721SS-PRF-2 (slide 16)
        if (freelist.head == freelist.tail && freelist.head_phase_bit == freelist.tail_phase_bit) {
            return true;
        }
        return false; 
    }

    bool renamer:: freelist_full(){ //721SS-PRF-2 (slide 16)
        if (freelist.head == freelist.tail && freelist.head_phase_bit != freelist.tail_phase_bit) {
            return true;
        }
        return false; 
    }

    void renamer::store_checkpoint(uint64_t branchID, uint64_t vpq_tail, bool vpq_tail_phase_bit) {
        //1. Handling Errors: BranchID must be within the valid checkpoint range
        if (branchID >= total_checkpoints) {
            cerr << "Error: Invalid branch ID for checkpoint." << endl;
            return;
        } 

        //2. Allocate memory for the shadow map table
        branchCheckpoint[branchID].shadowMapTable = new uint64_t[smt_size];

        //3. Copy the RMT to the shadow map table of the checkpoint
        for (uint64_t i = 0; i < smt_size; ++i) {
        branchCheckpoint[branchID].shadowMapTable[i] = rename_map_table[i];
        }

        //4. Copy the free list head and head phase bit
        branchCheckpoint[branchID].head = freelist.head;
        branchCheckpoint[branchID].head_phase_bit = freelist.head_phase_bit;

        //5. Store the GBM
        branchCheckpoint[branchID].checkpointedGBM = GBM;

        //6. Store VPQ tail entry and tail phase bit
        branchCheckpoint[branchID].vpq_tail = vpq_tail;
        branchCheckpoint[branchID].vpq_tail_phase_bit = vpq_tail_phase_bit;
    }

    bool renamer:: activelist_empty(){
        if (activeList.head == activeList.tail && activeList.head_phase_bit == activeList.tail_phase_bit) {
            return true;
        }
        return false; 
    }

    bool renamer:: activelist_full(){
        if (activeList.head == activeList.tail && activeList.head_phase_bit != activeList.tail_phase_bit) {
            return true;
        }
        return false; 
    }

    void renamer::activelist_initalize(ActiveListEntry *entry){
        entry->dest_existence = false; 
        entry->logical_dest = UINT64_MAX; 
        entry->physical_dest = UINT64_MAX; 
        entry->completed = false; 
        entry->exception = false;
        entry->load_violation = false; 
        entry->branch_mispredict = false; 
        entry->value_mispredict = false; 
        entry->load = false; 
        entry->store = false; 
        entry->branch = false; 
        entry->amo = false; 
        entry->csr = false; 
        entry->pc = UINT64_MAX;  
    }

    void renamer::checkpoint_initalize(BranchCheckpoint *point){
        point->shadowMapTable = {0}; 
        point->head = 0; 
        point->head_phase_bit = 0; 
        point->checkpointedGBM = 0; 
    }