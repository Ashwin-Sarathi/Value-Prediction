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
