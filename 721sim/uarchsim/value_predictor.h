#ifndef VALUE_PREDICTOR_H
#define VALUE_PREDICTOR_H

#include <cstdint>
#include <vector>
#include <queue>
#include <unordered_map>

using namespace std; 

// Maximum confidence level for prediction
const unsigned int CONF_MAX = 3;
const unsigned int REPLACE_THRESHOLD = 1; // Threshold to replace the prediction

	/////////////////////////////////////////////////////////////////////
	// Structure 1: Value Prediction Queue (VPQ) 
	/////////////////////////////////////////////////////////////////////

    // Define the structure for a value prediction entry.
    struct ValuePredictionEntry {
        uint64_t tag;             // The tag of the instruction
        unsigned int confidence;  // Confidence counter (saturates at 3)
        uint64_t retired_value;   // The last retired value of the instruction
        int64_t stride;          // The stride between consecutive values
        unsigned int instance;    // Latest Instance number in the pipeline
    };

    struct ValuePredictionQueue {
        uint64_t head, tail;                
        uint64_t size;                       // Size of the value prediction queue
        uint64_t head_phase_bit, tail_phase_bit; // Phase bits to distinguish full/empty states when head equals tail
        ValuePredictionEntry* queue;         // Array of value prediction entries


        // Constructor and Destructor
        ValuePredictionQueue(uint64_t size);
        ~ValuePredictionQueue();

        // Additional methods
        void enqueue(const ValuePredictionEntry& entry);
        bool dequeue(ValuePredictionEntry& entry);
        bool isConfidentPrediction(uint64_t tag) const;
        uint64_t getPredictedValue(uint64_t tag) const;
        void updatePrediction(uint64_t tag, uint64_t new_value, unsigned int new_confidence, uint64_t new_stride);
        void resetPrediction(uint64_t tag);
        void trainOrReplace(uint64_t tag, uint64_t value); // Trains or replaces the entry at retirement
        bool getPrediction(uint64_t tag, uint64_t &predicted_value); // Provides a prediction if available
    };

    //ValuePredictionQueue vpQueue; 



	/////////////////////////////////////////////////////////////////////
	// Structure 2: Stride Value Predictor (SVP) 
	/////////////////////////////////////////////////////////////////////







    /////////////////////////////////////////////////////////////////////
    // Additional functions to support value prediction in the pipeline
    /////////////////////////////////////////////////////////////////////

    //***************************************************************
    //Rename Stage
    //***************************************************************

    //Call from the rename stage to get predicted value
    bool get_confident_prediction(uint64_t logical_register, uint64_t& predicted_value);

// class vp {
//     public:
//     vp();
//     void vp_dump_stats(FILE* fp);
// };
#endif
=======
#include <inttypes.h>
#include <vector>
#include "parameters.h"

// Constants
const unsigned int CONF_MAX = 3; // Confidence level saturates at 3 
const unsigned int REPLACE_THRESHOLD = 1; // Threshold to replace a prediction entry

/////////////////////////////////////////////////////////////////////
// Structure 1: Stride Value Predictor Entry (SVP Entry)
/////////////////////////////////////////////////////////////////////
struct SVPEntry {
    uint64_t tag;               // Program Counter as the tag
    uint64_t last_value;            // Last retired value 
    int64_t stride;            // Stride 
    unsigned int confidence;   // Confidence counter
    unsigned int instance;     // Instance number in the pipeline
};

/////////////////////////////////////////////////////////////////////
// Structure 2: Stride Value Predictor (SVP)
/////////////////////////////////////////////////////////////////////
struct StrideValuePredictor {
    SVPEntry *table;           // Table of value predictions
    uint64_t size;             // Size of the SVP table

    // Constructor and Destructor
    StrideValuePredictor(uint64_t index_bits);
    ~StrideValuePredictor();

    // SVP Operations
    void trainOrReplace(uint64_t pc, uint64_t value); // Train or replace the SVP entry
    bool getPrediction(uint64_t pc, uint64_t& predicted_value); // Retrieve prediction if confidence is high
    bool getOraclePrediction(uint64_t pc, uint64_t& predicted_value, uint64_t actual_value); // Retrieve oracle prediction 
    unsigned int countVPQInstances(uint64_t pc); 
    uint64_t extractIndex(uint64_t pc); 
    uint64_t extractTag(uint64_t pc); 

};

StrideValuePredictor SVP(svp_index_bits); 

/////////////////////////////////////////////////////////////////////
// Structure 3: Value Prediction Queue (VPQ)
/////////////////////////////////////////////////////////////////////
struct VPQEntry {
    uint64_t pc;               // Program Counter associated with this prediction
    uint64_t computed_value; 
};

struct ValuePredictionQueue {
    uint64_t head, tail;       // Head and tail indices for the VPQ
    uint64_t head_phase_bit, tail_phase_bit; // Phase bits for head and tail
    VPQEntry *queue;           // Array for the VPQ
    uint64_t size;             // Size of the VPQ

    // Constructor and Destructor
    ValuePredictionQueue(uint64_t vpq_entries);
    ~ValuePredictionQueue();

    // VPQ Operations
    bool enqueue(uint64_t pc);
    bool dequeue(uint64_t& pc);
    bool isFull() const;
    bool isEmpty() const;
    bool stall_VPQ(uint64_t bundle_inst); 
};

ValuePredictionQueue VPQ(vpq_size); 

/////////////////////////////////////////////////////////////////////
// Additional functions to support value prediction in the pipeline
/////////////////////////////////////////////////////////////////////

// Function to be called from the rename stage to get predicted value
bool get_confident_prediction(uint64_t pc, uint64_t& predicted_value);

//Get Eligible instructions
bool isEligible(uint64_t pc, bool eligibility, bool destination_register);

bool get_oracle_confident_prediction(uint64_t pc, uint64_t& predicted_value, uint64_t actual_value);

#endif
