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
    // Calculate the index for the SVP entry using the PC
    uint64_t index = pc % SVP.size; // Modulo operation to wrap around the table size

    // If PC tags match or if the confidence level is 0, it needs to be replaced
    if (SVP.table[index].pc == pc || SVP.table[index].confidence == 0) {
        // If tag match or entry is unused, train the existing entry

        // Check if the current entry has the same stride as previous
        int64_t new_stride = value - SVP.table[index].last_value;

        if (SVP.table[index].stride == new_stride) {
            // If its the same stride -->  increase confidence while not exceeding CONF_MAX
            if (SVP.table[index].confidence < CONF_MAX) {
                SVP.table[index].confidence++;
            }
        } else {
            // If the stride has changed --> set the new stride and reset confidence
            SVP.table[index].stride = new_stride;
            SVP.table[index].confidence = 1; // Reset confidence to 1 as this is a new pattern
        }

        // Update last retired value and instance count
        SVP.table[index].last_value = value;
        SVP.table[index].instance = 1; // instance needs to be reset to 1 for a new observationas since we saw a new value

    } else if (SVP.table[index].confidence <= REPLACE_THRESHOLD) {
        // If the entry's confidence is less than or equal to threashold to replace --> replace the old prediction with the new one.

        SVP.table[index].pc = pc;
        SVP.table[index].last_value = value;
        SVP.table[index].stride = 0; // Initilaze the stride to 0
        SVP.table[index].confidence = 1; // Starting confidence for a new prediction
        SVP.table[index].instance = 1; // Start with the first instance
    }
}

bool StrideValuePredictor::getPrediction(uint64_t pc, uint64_t& predicted_value) {
    for (uint64_t i = 0; i < SVP.size; ++i) {
        if (SVP.table[i].pc == pc && SVP.table[i].confidence >= CONF_MAX) {
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

bool isEligible(uint64_t pc, bool eligibility) {
    if (eligibility) {
        // If the instruction is a branch, it's not eligible for value prediction
        return false;
    }

    // Search for the entry with the given PC.
    for (uint64_t i = 0; i < SVP.size; ++i) {
        if (SVP.table[i].pc == pc) {
            // Check if the confidence level of the prediction is saturated.
            return SVP.table[i].confidence >= CONF_MAX;
        }
    }
    return false;
}






