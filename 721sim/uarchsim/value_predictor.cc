#include "value_predictor.h"
#include <algorithm>
#include <cassert>
#include <iostream>

using namespace std; 

//-------------------------------------------------------------------
//Functions to manage VPQ
//-------------------------------------------------------------------

// Constructor for ValuePredictionQueue
ValuePredictionQueue::ValuePredictionQueue(uint64_t size) {
    vpQueue.head = 0;
    vpQueue.tail = 0;
    vpQueue.head_phase_bit = 0;
    vpQueue.tail_phase_bit = 0;
    vpQueue.size = size;
    vpQueue.queue = new ValuePredictionEntry[size];
}

// Destructor for ValuePredictionQueue
ValuePredictionQueue::~ValuePredictionQueue() {
    delete[] vpQueue.queue;
}

// Enqueue a new prediction entry
void ValuePredictionQueue::enqueue(const ValuePredictionEntry& entry) {
    // Ensure the queue is not full
    assert(!((vpQueue.head == vpQueue.tail) && (vpQueue.head_phase_bit != vpQueue.tail_phase_bit)));
    vpQueue.queue[vpQueue.tail] = entry;
    vpQueue.tail = (vpQueue.tail + 1) % vpQueue.size;
    if (vpQueue.tail == vpQueue.head) {
        // Toggle the phase bit to indicate the queue is full/empty
        vpQueue.tail_phase_bit = (vpQueue.tail_phase_bit == 0) ? 1 : 0;
    }
}

// Dequeue a prediction entry
bool ValuePredictionQueue::dequeue(ValuePredictionEntry& entry) {
    if ((vpQueue.head == vpQueue.tail) && (vpQueue.head_phase_bit == vpQueue.tail_phase_bit)) {
        return false; // Queue is empty
    }
    entry = vpQueue.queue[vpQueue.head];
    vpQueue.head = (vpQueue.head + 1) % vpQueue.size;
    if (vpQueue.head == vpQueue.tail) {
        // Toggle the phase bit to indicate the queue is full/empty
        vpQueue.head_phase_bit = (vpQueue.head_phase_bit == 0) ? 1 : 0;
    }
    return true;
}

// Check if there is a confident prediction for the given tag
bool ValuePredictionQueue::isConfidentPrediction(uint64_t tag) const {
    for (size_t i = vpQueue.head; ; i = (i + 1) % vpQueue.size) {
        if (vpQueue.queue[i].tag == tag && vpQueue.queue[i].confidence >= 3) {
            return true;
        }
        if (i == vpQueue.tail && vpQueue.head_phase_bit == vpQueue.tail_phase_bit) break; // End of queue
    }
    return false;
}

// Get the predicted value for the given tag
uint64_t ValuePredictionQueue::getPredictedValue(uint64_t tag) const {
    for (size_t i = vpQueue.head; ; i = (i + 1) % vpQueue.size) {
        if (vpQueue.queue[i].tag == tag) {
            // Calculate the predicted value based on retired_value, instance, and stride
            return vpQueue.queue[i].retired_value + vpQueue.queue[i].instance * vpQueue.queue[i].stride;
        }
        if (i == vpQueue.tail && vpQueue.head_phase_bit == vpQueue.tail_phase_bit) break; // End of queue
    }
    assert(false); // This should never happen if predictions are managed correctly
    return 0; 
}

// Update the prediction entry for the given tag
void ValuePredictionQueue::updatePrediction(uint64_t tag, uint64_t new_value, unsigned int new_confidence, uint64_t new_stride) {
    for (size_t i = vpQueue.head; ; i = (i + 1) % vpQueue.size) {
        if (vpQueue.queue[i].tag == tag) {
            vpQueue.queue[i].retired_value = new_value;
            vpQueue.queue[i].confidence = min(new_confidence, 3u); // Saturate confidence at 3
            vpQueue.queue[i].stride = new_stride;
            return;
        }
        if (i == vpQueue.tail && vpQueue.head_phase_bit == vpQueue.tail_phase_bit) break; // End of queue
    }
    assert(false); // This should never happen if predictions are managed correctly
}

// Reset the prediction entry for the given tag
void ValuePredictionQueue::resetPrediction(uint64_t tag) {
    for (size_t i = vpQueue.head; ; i = (i + 1) % vpQueue.size) {
        if (vpQueue.queue[i].tag == tag) {
            vpQueue.queue[i].confidence = 0; // Reset confidence to zero
            return;
        }
        if (i == vpQueue.tail && vpQueue.head_phase_bit == vpQueue.tail_phase_bit) break; // End of queue
    }
}


//-------------------------------------------------------------------
//Functions to inject confident value predictions
//-------------------------------------------------------------------

//*****************************
//Rename Stage
//*****************************

bool get_confident_prediction(uint64_t logical_register, uint64_t& predicted_value) {
    if (vpQueue.isConfidentPrediction(logical_register)) {
        // Get predicted value
        predicted_value = vpQueue.getPredictedValue(logical_register);
        return true;
    }
    //Do not update the predicted_value if its not confident
    return false;
}

//*****************************
//Dispatch Stage
//*****************************


//*****************************
//Writeback Stage
//*****************************



