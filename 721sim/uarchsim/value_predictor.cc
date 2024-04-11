#include "value_predictor.h"
#include <algorithm>
#include <cassert>
#include <iostream>

using namespace std; 

//-------------------------------------------------------------------
// Constructor and Destructor for StrideValuePredictor
//-------------------------------------------------------------------

StrideValuePredictor::StrideValuePredictor(uint64_t size) {
    SVP.size = size;
    table = new SVPEntry[size];
}

StrideValuePredictor::~StrideValuePredictor() {
    delete[] table;
}

//-------------------------------------------------------------------
// SVP Operations
//-------------------------------------------------------------------

void StrideValuePredictor::trainOrReplace(uint64_t pc, uint64_t value) {
}

bool StrideValuePredictor::getPrediction(uint64_t pc, uint64_t& predicted_value) {
    for (uint64_t i = 0; i < size; ++i) {
        if (table[i].pc == pc && table[i].confidence >= CONF_MAX) {
            predicted_value = table[i].last_value + (table[i].stride * table[i].instance);
            return true;
        }
    }
    return false;
}

//-------------------------------------------------------------------
// Constructor and Destructor for ValuePredictionQueue
//-------------------------------------------------------------------

ValuePredictionQueue::ValuePredictionQueue(uint64_t size) {
    VPQ.size = size;
    head = 0;
    tail = 0;
    head_phase_bit = 0;
    tail_phase_bit = 0;
    queue = new VPQEntry[size];
}

ValuePredictionQueue::~ValuePredictionQueue() {
    delete[] queue;
}

//-------------------------------------------------------------------
// VPQ Operations
//-------------------------------------------------------------------

void ValuePredictionQueue::enqueue(const SVPEntry& prediction, uint64_t pc) {
    // Make sure queue is not full
    assert(!isFull());
    queue[tail].prediction = prediction;
    queue[tail].pc = pc;
    tail = (tail + 1) % size;
    if (tail == head) {
        tail_phase_bit = 1 - tail_phase_bit; // Toggle the phase bit
    }
}

bool ValuePredictionQueue::dequeue(SVPEntry& prediction, uint64_t& pc) {
    if (isEmpty()) {
        return false; // Queue is empty
    }
    prediction = queue[head].prediction;
    pc = queue[head].pc;
    head = (head + 1) % size;
    if (head == tail) {
        head_phase_bit = 1 - head_phase_bit; // Toggle the phase bit
    }
    return true;
}

bool ValuePredictionQueue::isFull() const {
    return (head == tail) && (head_phase_bit != tail_phase_bit);
}

bool ValuePredictionQueue::isEmpty() const {
    return (head == tail) && (head_phase_bit == tail_phase_bit);
}

//-------------------------------------------------------------------
// Additional functions to support value prediction in the pipeline
//-------------------------------------------------------------------

bool get_confident_prediction(uint64_t pc, uint64_t& predicted_value) {
    return SVP.getPrediction(pc, predicted_value);
}

void send_predicted_value_to_issue_queue(uint64_t logical_register, uint64_t predicted_value) {
    // Implementation goes here
}
