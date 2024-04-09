#ifndef VALUE_PREDICTOR_H
#define VALUE_PREDICTOR_H

#include <cstdint>
#include <vector>
#include <queue>
#include <unordered_map>

using namespace std; 


	/////////////////////////////////////////////////////////////////////
	// Structure 1: Value Prediction Queue 
	/////////////////////////////////////////////////////////////////////

    // Define the structure for a value prediction entry.
    struct ValuePredictionEntry {
        uint64_t tag;             // The tag of the instruction
        unsigned int confidence;  // Confidence counter (saturates at 3)
        uint64_t retired_value;   // The last retired value of the instruction
        uint64_t stride;          // The stride between consecutive values
        unsigned int instance;    // Latest Instance number in the pipeline
    };

    struct ValuePredictionQueue {
        uint64_t head, tail;                
        uint64_t size;                       // Size of the value prediction queue
        bool head_phase_bit, tail_phase_bit; // Phase bits to distinguish full/empty states when head equals tail
        ValuePredictionEntry* queue;         // Array of value prediction entries


        // Additional methods
        void enqueue(const ValuePredictionEntry& entry);
        bool dequeue(ValuePredictionEntry& entry);
        bool isConfidentPrediction(uint64_t tag) const;
        uint64_t getPredictedValue(uint64_t tag) const;
        void updatePrediction(uint64_t tag, uint64_t new_value, unsigned int new_confidence, uint64_t new_stride);
        void resetPrediction(uint64_t tag);
    };

    ValuePredictionQueue vpQueue; 

    /////////////////////////////////////////////////////////////////////
    // Additional functions to support value prediction in the pipeline
    /////////////////////////////////////////////////////////////////////

    //***************************************************************
    //Rename Stage
    //***************************************************************

    //Call from the rename stage to get predicted value
    bool get_confident_prediction(uint64_t logical_register, uint64_t& predicted_value);

    //***************************************************************
    //Dispatch Stage
    //***************************************************************

    //Write predicted values into the physical register file
    void write_predicted_values_to_prf(uint64_t physical_register, uint64_t value);

    //Set PRF ready bits of predicted destination registers
    void set_ready_bits_of_predicted_registers(uint64_t physical_register);

    //Send predicted value to the issue queue
    void send_predicted_value_to_issue_queue(uint64_t logical_register, uint64_t predicted_value);



#endif
