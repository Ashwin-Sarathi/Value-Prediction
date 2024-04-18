#include "value_predictor.h"
#include <algorithm>
#include <cassert>
#include <iostream>

using namespace std;

//-------------------------------------------------------------------
// Constructor and Destructor for svp_vpq
//-------------------------------------------------------------------

svp_vpq::svp_vpq(uint64_t svp_index_bits, uint64_t vpq_entries) {
    svp_size = 1ULL << svp_index_bits; // 2^index_bits
    svp_table = new SVPEntry[svp_size];

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

    if (svp_table[index].tag == tag || svp_table[index].tag == 0) {        //SVP hit or no tag
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
                //svp_table[index].confidence = (svp_conf_dec == 0) ? 0 : max(svp_table[index].confidence - svp_conf_dec, 0u);
            }
            svp_table[index].confidence = (svp_conf_dec == 0) ? 0 : max(svp_table[index].confidence - svp_conf_dec, 0u);
        }
        svp_table[index].last_value = value;
        // Decrement the instance counter
        if (svp_table[index].instance > 0) {
            svp_table[index].instance--;
        }
    } else {
        if (svp_table[index].confidence <= svp_replace) {
            svp_table[index].tag = tag;            
            svp_table[index].stride = value;
            svp_table[index].last_value = value;
            svp_table[index].confidence = 0; 
            svp_table[index].instance = countVPQInstances(pc) - 1;
        }
    }

    // Incrementing VPQ head pointer
    dequeue(pc);
}

unsigned int svp_vpq::countVPQInstances(uint64_t pc) {
    unsigned int count = 0;
    uint64_t temp_vpq_head = vpq_head;
    bool temp_vpq_head_phase_bit = vpq_head_phase_bit;
    while (temp_vpq_head != vpq_tail) {

        if (vpq_queue[temp_vpq_head].pc == generateVPQEntryPC(pc)) {
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
    return count;
}

//Extract index from the PC, based on the number of index bits
uint64_t svp_vpq::extractIndex(uint64_t pc) {
    return (pc >> 2) & ((1ULL << svp_index_bits) - 1);
}

//Extract tag from the PC, based on the number of tag bits
uint64_t svp_vpq::extractTag(uint64_t pc) {
    return (pc >> (2 + svp_index_bits)) & ((1ULL << svp_tag_bits) - 1);
}

bool svp_vpq::getPrediction(uint64_t pc, uint64_t& predicted_value) {
    uint64_t tag = extractTag(pc); // Use a method to extract the tag based on PC
    for (uint64_t i = 0; i < svp_size; ++i) {
        if (svp_table[i].tag == tag && svp_table[i].confidence == svp_conf_max) {
            predicted_value = svp_table[i].last_value + (svp_table[i].stride * svp_table[i].instance);
            svp_table[i].instance++;            //increment the instance 
            return true;
        }
    }

    // No SVP hit or if conf != conf_max
    return false;
}

bool svp_vpq::getOraclePrediction(uint64_t pc, uint64_t& predicted_value, uint64_t debugger_value) {
    uint64_t tag = extractTag(pc); // Use a method to extract the tag based on PC

    for (uint64_t i = 0; i < svp_size; ++i) {

        if (svp_table[i].tag == tag) {          //Generate a prediction every time we get an SVP hit
            predicted_value = svp_table[i].last_value + (svp_table[i].stride * svp_table[i].instance);
            if(predicted_value == debugger_value){
                svp_table[i].instance++;
                return true;
            }  
        }
    }

    //No SVP hit or wrong prediction 
    return false;
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
    // if (isVPQFull()) {
    //     std::cerr << "ERROR: Attempted to enqueue to a full Value Prediction Queue.\n";
    //     return false;
    // }
    assert(!isVPQFull());
    unsigned int old_tail = vpq_tail;

    uint64_t vpq_pc = generateVPQEntryPC(pc);
    vpq_queue[vpq_tail].pc = vpq_pc;
    vpq_tail++;
    
    // Handle wrap-around case for the tail pointer
    if (vpq_tail == vpq_size) {
        vpq_tail_phase_bit = !vpq_tail_phase_bit;
        vpq_tail = 0;
    }
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

uint64_t svp_vpq::retComputedValue(uint64_t index){
    return vpq_queue[index].computed_value; 
}

//-------------------------------------------------------------------
// Additional functions to support value prediction in the pipeline
//-------------------------------------------------------------------

bool svp_vpq::getConfidentPrediction(uint64_t pc, uint64_t& predicted_value) {
    return getPrediction(pc, predicted_value);
}

bool svp_vpq::isEligible(uint64_t pc, bool is_branch, bool destination_register) {
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

    if(!svp_predict_int_alu) {
        return false;
    }

    if (!svp_predict_fp_alu) {
        return false;
    }

    if (!svp_predict_load) {
        return false;
    }
    return true;
}

bool svp_vpq::getOracleConfidentPrediction(uint64_t pc, uint64_t& predicted_value, uint64_t actual_value) {
    return getOraclePrediction(pc, predicted_value, actual_value);
}

bool svp_vpq::comparePredictedAndComputed(uint64_t predicted_value, uint64_t computed_value) {
    return (predicted_value == computed_value);
}