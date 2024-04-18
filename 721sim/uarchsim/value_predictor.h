#ifndef VALUE_PREDICTOR_H
#define VALUE_PREDICTOR_H

#include <inttypes.h>
#include <vector>
#include "parameters.h"

/////////////////////////////////////////////////////////////////////
// Structure 1: Stride Value Predictor Entry (SVP Entry)
/////////////////////////////////////////////////////////////////////
struct SVPEntry {
    uint64_t tag;             // Program Counter as the tag
    uint64_t last_value;      // Last retired value 
    int64_t stride;           // Stride 
    unsigned int confidence;  // Confidence counter
    unsigned int instance;    // Instance number in the pipeline
};

/////////////////////////////////////////////////////////////////////
// Structure 2: Value Prediction Queue (VPQ) Entry
/////////////////////////////////////////////////////////////////////
struct VPQEntry {
    uint64_t pc;              // Program Counter associated with this prediction
    uint64_t computed_value;  // Computed value for prediction
};

/////////////////////////////////////////////////////////////////////
// Class: svp_vpq
/////////////////////////////////////////////////////////////////////
class svp_vpq {
    private:
        SVPEntry *svp_table;     // Table of SVP entries
        VPQEntry *vpq_queue;     // VPQ entries
        uint64_t svp_size;       // Size of the SVP table
        uint64_t vpq_size;       // Size of the VPQ
        uint64_t vpq_head, vpq_tail; // Head and tail indices for the VPQ
        bool vpq_head_phase_bit, vpq_tail_phase_bit; // Phase bits for head and tail of VPQ

    public:
        // Constructor and Destructor
        svp_vpq(uint64_t svp_index_bits, uint64_t vpq_entries);
        ~svp_vpq();

        // SVP Operations
        void trainOrReplace(uint64_t pc, uint64_t value); // Train or replace the SVP entry
        bool getPrediction(uint64_t pc, uint64_t& predicted_value); // Retrieve prediction if confidence is high
        bool getOraclePrediction(uint64_t pc, uint64_t& predicted_value, uint64_t actual_value); // Retrieve oracle prediction 
        unsigned int countVPQInstances(uint64_t pc); 
        uint64_t extractIndex(uint64_t pc); 
        uint64_t extractTag(uint64_t pc); 

        // VPQ Operations
        int enqueue(uint64_t pc);
        bool dequeue(uint64_t pc);
        bool isVPQFull() const;
        bool isVPQEmpty() const;
        bool stallVPQ(uint64_t bundle_inst);
        uint64_t generateVPQEntryPC(uint64_t pc);
        void getTailForCheckpoint(uint64_t &tail, bool &tail_phase_bit);
        void addComputedValueToVPQ(unsigned int vpq_index, uint64_t computed_value);
        void rollBackVPU();
        uint64_t retComputedValue(uint64_t pc);

        // Additional functions to support value prediction in the pipeline
        bool getConfidentPrediction(uint64_t pc, uint64_t& predicted_value);
        bool isEligible(uint64_t pc, bool eligibility, bool destination_register);
        bool getOracleConfidentPrediction(uint64_t pc, uint64_t& predicted_value, uint64_t actual_value);
        bool comparePredictedAndComputed(uint64_t predicted_value, uint64_t computed_value);
};

#endif // VALUE_PREDICTOR_H