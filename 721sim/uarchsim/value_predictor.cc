#include "value_predictor.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <cstdio>

using namespace std;

//-------------------------------------------------------------------
// Constructor and Destructor for svp_vpq
//-------------------------------------------------------------------

svp_vpq::svp_vpq(uint64_t svp_index_bits, uint64_t vpq_entries) {
    svp_size = 1ULL << svp_index_bits; // 2^index_bits
    svp_table = new SVPEntry[svp_size];

    for(int i=0; i<svp_size; i++){
        svp_table[i].tag  = 0; 
        svp_table[i].last_value = 0; 
        svp_table[i].stride = 0; 
        svp_table[i].confidence = 0; 
        svp_table[i].instance = 0; 
    }

    vpq_size = vpq_entries;
    vpq_head = 0;
    vpq_tail = 0;
    vpq_head_phase_bit = 0;
    vpq_tail_phase_bit = 0;
    vpq_queue = new VPQEntry[vpq_size];
}

svp_vpq::~svp_vpq() {
    delete[] svp_table;
    delete[] vpq_queue;
}

//-------------------------------------------------------------------
// SVP Operations
//-------------------------------------------------------------------

void svp_vpq::trainOrReplace(uint64_t pc, uint64_t value) {  
    uint64_t index = extractIndex(pc); // Use a method to extract the index based on PC
    uint64_t tag = extractTag(pc); // Use a method to extract the tag based on PC

    if (svp_table[index].tag == tag) {        //SVP hit or no tag
        // Existing entry, check for stride consistency
        int64_t new_stride = value - svp_table[index].last_value;
        if (svp_table[index].stride == new_stride) {
            // If stride hasn't changed, potentially increase confidence
            svp_table[index].confidence = min(static_cast<int>(svp_table[index].confidence + svp_conf_inc), svp_conf_max);
        } else {
            // New stride observed
            if (svp_table[index].confidence <= svp_replace_stride) {
                // Only update stride if confidence is less than or equal to the replace_stride threshold
                svp_table[index].stride = new_stride;
            }
            svp_table[index].confidence = (svp_conf_dec == 0) ? 0 : max(svp_table[index].confidence - svp_conf_dec, 0u);
        }
        svp_table[index].last_value = value;
        // Decrement the instance counter
        if (svp_table[index].instance > 0) {
            svp_table[index].instance--;
            assert(svp_table[index].instance >= 0);
        }
    } else {
        if (svp_table[index].confidence <= svp_replace) {
            svp_table[index].tag = tag;            
            svp_table[index].stride = value;
            svp_table[index].last_value = value;
            svp_table[index].confidence = 0; 
            svp_table[index].instance = countVPQInstances(pc);
        }
    }

    //Incrementing VPQ head pointer
    dequeue(pc);
}

unsigned int svp_vpq::countVPQInstances(uint64_t pc) {
    // cout << "Counting" << endl;
    unsigned int count = 0;
    uint64_t temp_vpq_head = vpq_head;
    bool temp_vpq_head_phase_bit = vpq_head_phase_bit;
    while (temp_vpq_head != vpq_tail) {

        if (vpq_queue[temp_vpq_head].pc == pc) {
            count++;
        }
        
        // Move the head pointer
        temp_vpq_head++;
        
        // If the head wraps around --> toggle the phase bit
        if (temp_vpq_head == vpq_size) {
            temp_vpq_head_phase_bit = !temp_vpq_head_phase_bit;
            temp_vpq_head = 0;
        }
    }
    return (count == 0) ? 0 : (count - 1);
}

//Extract index from the PC, based on the number of index bits
uint64_t svp_vpq::extractIndex(uint64_t pc) {
    return (pc >> 2) & ((1ULL << svp_index_bits) - 1);
}

//Extract tag from the PC, based on the number of tag bits
uint64_t svp_vpq::extractTag(uint64_t pc) {
    return (pc >> (2 + svp_index_bits)) & ((1ULL << svp_tag_bits) - 1);
}

void svp_vpq::printSVPStatus() {
    fprintf(stdout, "SVP Entries:\n");
    fprintf(stdout, "SVP entry #:\ttag(hex)\tconf\tretired_value\tstride\tinstance\n");
    for (uint64_t i = 0; i < svp_size; ++i) {
        const auto& entry = svp_table[i];
        fprintf(stdout, "%10lu:\t%8lx\t%4u\t%12lu\t%8ld\t%4u\n",
                i, entry.tag, entry.confidence, entry.last_value, entry.stride, entry.instance);
    }
}

//-------------------------------------------------------------------
// VPQ Operations
//-------------------------------------------------------------------

uint64_t svp_vpq::generateVPQEntryPC(uint64_t pc) {
    uint64_t tag = extractTag(pc);
    uint64_t index = extractIndex(pc);
    pc = (tag << svp_index_bits) | (index); 
    return pc;
}

int svp_vpq::enqueue(uint64_t pc) {

    //VPQ should not be full at this stage
    assert(!isVPQFull());
    
    //Counter to manage VPQ_index 
    unsigned int old_tail = vpq_tail;

    //Store PC information 
    vpq_queue[vpq_tail].pc = pc;
    vpq_queue[vpq_tail].PCtag = extractTag(pc);
    vpq_queue[vpq_tail].PCindex = extractIndex(pc);

    //Increment tail pointer 
    vpq_tail++;
    
    //Handle wrap-around case for the tail pointer
    if (vpq_tail == vpq_size) {
        vpq_tail_phase_bit = !vpq_tail_phase_bit;
        vpq_tail = 0;
    }

    //return VPQ_index for this instruction 
    return old_tail;
}

bool svp_vpq::dequeue(uint64_t pc) {
    // if (isVPQEmpty()) {
    //     std::cerr << "ERROR: Attempted to dequeue from an empty Value Prediction Queue.\n";
    //     return false;
    // }
    assert(!isVPQEmpty());

    // pc = vpq_queue[vpq_head].pc;
    vpq_head++;
    
    // Handle wrap-around case for the head pointer
    if (vpq_head == vpq_size) {
        vpq_head_phase_bit = !vpq_head_phase_bit;
        vpq_head = 0;
    }
    return true;
}

bool svp_vpq::isVPQFull() const {
    return (vpq_head == vpq_tail) && (vpq_head_phase_bit != vpq_tail_phase_bit);
}

bool svp_vpq::isVPQEmpty() const {
    return (vpq_head == vpq_tail) && (vpq_head_phase_bit == vpq_tail_phase_bit);
}

bool svp_vpq::stallVPQ(uint64_t bundle_inst) {
    uint64_t free_entries;

    // If VPQ is full --> stall 
    if (isVPQFull()) {
        return true;
    }
    else if (isVPQEmpty()) {
        free_entries = vpq_size;
    }
    // If head and tail are in the same phase, tail is ahead of head
    else if (vpq_head_phase_bit == vpq_tail_phase_bit) {
        free_entries = vpq_size - (vpq_tail - vpq_head);
    }
    // If head and tail are in different phases, head is ahead of tail
    else if (vpq_head_phase_bit != vpq_tail_phase_bit) {
        free_entries = vpq_head - vpq_tail;
    }
    // Should never reach here if VPQ is properly managed
    else {
        free_entries = static_cast<uint64_t>(-1); 
        assert(false);
    }
    // Check if there is enough space for the bundle of instructions
    if (free_entries < bundle_inst) {
        return true; 
    }
    return false; 
}

void svp_vpq::getTailForCheckpoint(uint64_t &tail, bool &tail_phase_bit) {
    tail = vpq_tail;
    tail_phase_bit = vpq_tail_phase_bit;
    return;    
}

void svp_vpq::addComputedValueToVPQ(unsigned int vpq_index, uint64_t computed_value) {
    vpq_queue[vpq_index].computed_value = computed_value;
}

uint64_t svp_vpq::getComputedValue(uint64_t index) {
    return vpq_queue[index].computed_value; 
}

void svp_vpq::getHeadAtRetire(uint64_t &pc, uint64_t &value) {
    pc = vpq_queue[vpq_head].pc;
    value = vpq_queue[vpq_head].computed_value;
}

void svp_vpq::printVPQStatus() {

    fprintf(stdout, "VPQ State:\n");
    fprintf(stdout, "Head: %u (Phase Bit: %d)\n", vpq_head, vpq_head_phase_bit);
    fprintf(stdout, "Tail: %u (Phase Bit: %d)\n", vpq_tail, vpq_tail_phase_bit);
    fprintf(stdout, "\n");

    fprintf(stdout, "VPQ Entries:\n");
    fprintf(stdout, "VPQ entry #:\tPC(hex)\tPCtag(hex)\tPCindex(hex)\tComputed Value\n");

    unsigned int i = vpq_head;
    while (true) {
        const auto& entry = vpq_queue[i];
        fprintf(stdout, "%u\t%lx\t%lx\t%lx\t%lu\n", i, entry.pc, entry.PCtag, entry.PCindex, entry.computed_value);

        if (i == vpq_tail) {
            break; 
        }

        i = (i + 1) % vpq_size; //wrap around at end of queue 

        if (i == vpq_head) {
            break; 
        }
    }
}


//-----------------------------------
// Functions to manage VPU rollback
//-----------------------------------
void svp_vpq::fullSquashVPU() {
    // For a full rollback of VPQ, the VPQ tail is set to the VPQ head
    vpq_tail = vpq_head;
    vpq_tail_phase_bit = vpq_head_phase_bit;

    // The SVP is scanned and all instances are set to 0
    for (uint64_t i = 0; i < svp_size; ++i) {
        svp_table[i].instance = 0;
    }
}

void svp_vpq::partialRollbackVPU(uint64_t checkpointed_tail, bool checkpointed_tail_phase_bit) {
    if ((checkpointed_tail == vpq_tail) && (checkpointed_tail_phase_bit == vpq_tail_phase_bit)) {
        return;  // No rollback needed as the states match
    }

    // Full squash if tail matches head with the same phase bit
    if ((checkpointed_tail == vpq_head) && (checkpointed_tail_phase_bit == vpq_head_phase_bit)) {
        fullSquashVPU();
        return;
    }

    // Rollback the VPQ to the checkpointed tail
    while ((vpq_tail != checkpointed_tail) || (vpq_tail_phase_bit != checkpointed_tail_phase_bit)) {
        // Decrement vpq_tail before modifying to ensure the current tail entry is correctly handled
        if (vpq_tail == 0) {
            vpq_tail = vpq_size - 1;
            vpq_tail_phase_bit = !vpq_tail_phase_bit;  // Toggle phase bit on wrap-around
        } else {
            vpq_tail--;
        }

        // Invalidate the current tail entry (pointed at by the decremented vpq_tail)
        uint64_t index = vpq_queue[vpq_tail].PCindex;
        uint64_t tag = vpq_queue[vpq_tail].PCtag;
        if (svp_table[index].tag == tag && svp_table[index].instance > 0) {
            svp_table[index].instance--;
        }

        // Optionally clear the VPQ entry if needed
        vpq_queue[vpq_tail].pc = 0;  // Clear the pc to indicate it's no longer valid
        vpq_queue[vpq_tail].PCtag = 0;  // Clear additional fields as needed
        vpq_queue[vpq_tail].PCindex = 0;
    }

    // Confirm phase bit matches the checkpointed state after rollback
    assert(vpq_tail_phase_bit == checkpointed_tail_phase_bit);
}

//-------------------------------------------------------------------
// Additional functions to support value prediction in the pipeline
//-------------------------------------------------------------------

int svp_vpq::getRealPrediction(uint64_t pc, uint64_t& predicted_value) {
    uint64_t tag = extractTag(pc); // Use a method to extract the tag based on PC
    uint64_t index = extractIndex(pc);

        if (svp_table[index].tag == tag) {
            svp_table[index].instance ++;            // Increment the instance counter
            if (svp_table[index].confidence == svp_conf_max) {
                predicted_value = svp_table[index].last_value + (svp_table[index].stride * svp_table[index].instance);  
                return 2;                            // Tag match and max confidence
            }        
            predicted_value = svp_table[index].last_value + (svp_table[index].stride * svp_table[index].instance);           
            return 1;                                // Tag match but unconfident
        }

    // No SVP hit 
    return 0;
}

bool svp_vpq::isEligible(uint64_t pc, bool is_branch, bool destination_register, fu_type instruction_type, bool load) {
    uint64_t tag = extractTag(pc); // Use a method to extract the tag based on PC
    //check for destination register
    if(!destination_register){
        return false; 
    }

    //check for branches 
    if (is_branch) {
        // If the instruction is a branch, it's not eligible for value prediction
        return false;
    }

    // If we are not predicting int ALU instructions and given instruction matches that type, then we don't predict it
    if(!svp_predict_int_alu && (instruction_type == FU_ALU_S || instruction_type == FU_ALU_C)) {            
        return false;
    }

    // If we are not predicting FP ALU instructions and given instruction matches that type, then we don't predict it
    if (!svp_predict_fp_alu && instruction_type == FU_ALU_FP) {
        return false;
    }

    // If we are not predicting loads and given instruction is a load, then we don't predict it
    if (!svp_predict_load && load) {
        return false;
    }

    return true;
}

int svp_vpq::getOraclePrediction(uint64_t pc, uint64_t& predicted_value, uint64_t actual_value) {
    uint64_t tag = extractTag(pc); // Use a method to extract the tag based on PC
    uint64_t index = extractIndex(pc);

    if (svp_table[index].tag == tag) {          // Generate a prediction every time we get an SVP hit
        svp_table[index].instance++;
        predicted_value = svp_table[index].last_value + (svp_table[index].stride * svp_table[index].instance);
        if(predicted_value == actual_value) {
            return 2;                       // 2 implies tag match and value match
        }  
        else {
            return 1;                       // 1 imlpies tag match but value mismatch
        }
    }

    //No SVP hit or wrong prediction 
    return 0;                               // 0 implies no tag match and SVP miss
}

bool svp_vpq::comparePredictedAndComputed(uint64_t predicted_value, uint64_t computed_value) {
    return (predicted_value == computed_value);
}