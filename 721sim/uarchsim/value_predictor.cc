#include "value_predictor.h"
#include <algorithm>
#include <cassert>
#include <iostream>

using namespace std; 

//-------------------------------------------------------------------
// Constructor and Destructor for StrideValuePredictor
//-------------------------------------------------------------------

StrideValuePredictor::StrideValuePredictor(uint64_t index_bits) {
    SVP.size = 1ULL << index_bits; //2^index_bits
    SVP.table = new SVPEntry[SVP.size];
}

StrideValuePredictor::~StrideValuePredictor() {
    delete[] table;
}

//-------------------------------------------------------------------
// SVP Operations
//-------------------------------------------------------------------

void StrideValuePredictor::trainOrReplace(uint64_t pc, uint64_t value) {
    uint64_t index = extractIndex(pc); // Use a method to extract the index based on PC
    uint64_t tag = extractTag(pc); // Use a method to extract the tag based on PC

    if (table[index].tag == tag || table[index].tag == 0) {        //SVP hit or no tag
        // Existing entry, check for stride consistency
        int64_t new_stride = value - table[index].last_value;
        if (table[index].stride == new_stride) {
            // If stride hasn't changed, potentially increase confidence
            table[index].confidence = min(table[index].confidence + svp_conf_inc, svp_conf_max);
        } else {
            // New stride observed
            if (table[index].confidence <= svp_replace_stride) {
                // Only update stride if confidence is less than or equal to the replace_stride threshold
                table[index].stride = new_stride;
                table[index].confidence = (svp_conf_dec == 0) ? 0 : max(table[index].confidence - svp_conf_dec, 0u);
            }
        }
        table[index].last_value = value;

        // Decrement the instance counter
        if (table[index].instance > 0) {
            table[index].instance--;
        }
    } else {
        //Replacing Entry 
        // Entry is either unused or has low confidence
        if (table[index].confidence <= svp_replace) {
            table[index].tag = tag;
            table[index].last_value = value;
            table[index].stride = 0; // Initialize stride to 0 on new allocation
            table[index].confidence = 0; // Start with 0 confidence
            table[index].instance = countVPQInstances(pc);  // New entry, set instance to 1
        }
    }
}

unsigned int StrideValuePredictor::countVPQInstances(uint64_t pc) {
    unsigned int count = 0;
    for (unsigned int i = VPQ.head; i != VPQ.tail; i = (i + 1) % VPQ.size) {
        if (VPQ.queue[i].pc == pc) {
            count++;
        }
        // detect wrap-around in the VPQ.
        if (i == VPQ.tail && VPQ.head_phase_bit != VPQ.tail_phase_bit) break;
    }
    return count;
}

//Extract index from the PC, based on the number of index bits
uint64_t StrideValuePredictor::extractIndex(uint64_t pc) {
    return (pc >> 2) & ((1ULL << svp_index_bits) - 1);
}

//Extract tag from the PC, based on the number of tag bits
uint64_t StrideValuePredictor::extractTag(uint64_t pc) {
    return (pc >> (2 + svp_index_bits)) & ((1ULL << svp_tag_bits) - 1);
}

bool StrideValuePredictor::getPrediction(uint64_t pc, uint64_t& predicted_value) {
    uint64_t tag = extractTag(pc); // Use a method to extract the tag based on PC
    for (uint64_t i = 0; i < SVP.size; ++i) {
        if (SVP.table[i].tag == tag && SVP.table[i].confidence >= svp_conf_max) {
            predicted_value = SVP.table[i].last_value + (SVP.table[i].stride * SVP.table[i].instance);
            SVP.table[i].instance++;            //increment the instance 
            return true;
        }
    }
    return false;
}

//-------------------------------------------------------------------
// Constructor and Destructor for ValuePredictionQueue
//-------------------------------------------------------------------

ValuePredictionQueue::ValuePredictionQueue(uint64_t vpq_entries) {
    VPQ.size = vpq_entries;
    VPQ.head = 0;
    VPQ.tail = 0;
    VPQ.head_phase_bit = 0;
    VPQ.tail_phase_bit = 0;
    VPQ.queue = new VPQEntry[VPQ.size];
}

ValuePredictionQueue::~ValuePredictionQueue() {
    delete[] queue;
}

//-------------------------------------------------------------------
// VPQ Operations
//-------------------------------------------------------------------

bool ValuePredictionQueue::enqueue(uint64_t pc) {
    if (isFull()) {
        std::cerr << "ERROR: Attempted to enqueue to a full Value Prediction Queue.\n";
        return false;
    }

    queue[tail].pc = pc;
    tail++;
    
    // Handle wrap-around case for the tail pointer
    if (tail == size) {
        tail_phase_bit = !tail_phase_bit;
        tail = 0;
    }
    return true;
}

bool ValuePredictionQueue::dequeue(uint64_t& pc) {
    if (isEmpty()) {
        std::cerr << "ERROR: Attempted to dequeue from an empty Value Prediction Queue.\n";
        return false;
    }

    pc = queue[head].pc;
    head++;
    
    // Handle wrap-around case for the head pointer
    if (head == size) {
        head_phase_bit = !head_phase_bit;
        head = 0;
    }
    return true;
}

bool ValuePredictionQueue::isFull() const {
    return (head == tail) && (head_phase_bit != tail_phase_bit);
}

bool ValuePredictionQueue::isEmpty() const {
    return (head == tail) && (head_phase_bit == tail_phase_bit);
}

bool ValuePredictionQueue::stall_VPQ(uint64_t bundle_inst) {
    uint64_t free_entries;

    // If VPQ is full --> stall 
    if (isFull()) {
        return true;
    }
    else if (isEmpty()) {
        free_entries = size;
    }
    // If head and tail are in the same phase, tail is ahead of head
    else if (head_phase_bit == tail_phase_bit) {
        free_entries = size - (tail - head);
    }
    // If head and tail are in different phases, head is ahead of tail
    else if (head_phase_bit != tail_phase_bit) {
        free_entries = head - tail;
    }
    // Should never reach here if VPQ is properly managed
    else {
        free_entries = static_cast<uint64_t>(-1); 
        assert(false);
    }

    // Check if there is enough space for the bundle of instructions.
    if (free_entries < bundle_inst) {
        return true; 
    }
    return false; 
}


//-------------------------------------------------------------------
// Additional functions to support value prediction in the pipeline
//-------------------------------------------------------------------

bool get_confident_prediction(uint64_t pc, uint64_t& predicted_value) {
    return SVP.getPrediction(pc, predicted_value);
}

bool isEligible(uint64_t pc, bool eligibility, bool destination_register) {
    
    //check for destination register
    if(!destination_register){
        return false; 
    }

    //check for branches 
    if (eligibility) {
        // If the instruction is a branch, it's not eligible for value prediction
        return false;
    }

    if(enable_value_prediction){
        // Search for the entry with the given PC and check for confidence
        for (uint64_t i = 0; i < SVP.size; ++i) {
            if (SVP.table[i].pc == pc) {
                // Check if the confidence level of the prediction is saturated.
                return SVP.table[i].confidence >= CONF_MAX;
            }
        }
    }
    return false;
}






