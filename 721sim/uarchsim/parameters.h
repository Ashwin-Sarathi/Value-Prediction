#ifndef PARAMETERS_H
#define PARAMETERS_H
#include <cinttypes>

// Test vp stats params - REMOVE LATER
extern uint64_t vpmeas_ineligible;
extern uint64_t vpmeas_ineligible_type;
extern uint64_t vpmeas_ineligible_drop;
extern uint64_t vpmeas_eligible;
extern uint64_t vpmeas_miss;
extern uint64_t vpmeas_conf_corr;
extern uint64_t vpmeas_conf_incorr;
extern uint64_t vpmeas_unconf_corr;
extern uint64_t vpmeas_unconf_incorr;

// Pipe control
extern unsigned int PIPE_QUEUE_SIZE;

// Value Prediction
extern bool VALUE_PREDICTION_ENABLED;
extern bool PERFECT_VALUE_PREDICTION;
extern bool IMMEDIATE_VALUE_PREDICTION_RECOVERY;

// SVP Configurations
extern int vpq_size;
extern bool oracle_confidence;
extern int svp_index_bits;
extern int svp_tag_bits;
extern int svp_conf_max;
extern int svp_conf_inc;
extern int svp_conf_dec;
extern int svp_replace_stride;
extern int svp_replace;
extern bool svp_predict_int_alu;
extern bool svp_predict_fp_alu;
extern bool svp_predict_load;
extern bool vpq_full_policy;

extern int svp_conf_imm_rec;

// Oracle controls.
extern bool PERFECT_BRANCH_PRED;
extern bool PERFECT_TRACE_CACHE;
extern bool ORACLE_DISAMBIG;
extern bool PERFECT_ICACHE;
extern bool PERFECT_DCACHE;

// Core.
extern unsigned int FETCH_QUEUE_SIZE;
extern unsigned int NUM_CHECKPOINTS;
extern unsigned int ACTIVE_LIST_SIZE;
extern bool AUTO_PRF_SIZE;
extern unsigned int PRF_SIZE;
extern unsigned int ISSUE_QUEUE_SIZE;
extern unsigned int ISSUE_QUEUE_NUM_PARTS;
extern unsigned int LQ_SIZE;
extern unsigned int SQ_SIZE;
extern unsigned int FETCH_WIDTH;
extern unsigned int DISPATCH_WIDTH;
extern unsigned int ISSUE_WIDTH;
extern unsigned int RETIRE_WIDTH;
extern bool         IC_INTERLEAVED;
extern bool         IC_SINGLE_BB;		// not used currently
extern bool         IN_ORDER_ISSUE;		// not used currently
extern bool         SPEC_DISAMBIG;
extern bool         MEM_DEP_PRED;
extern bool         MDP_STICKY;
extern unsigned int MDP_MAX;
extern bool         SPLIT_STORES;
extern bool         PRESTEER;
extern bool         IDEAL_AGE_BASED;
extern unsigned int FU_LANE_MATRIX[];
extern unsigned int FU_LAT[];

// L1 Data Cache.
extern unsigned int L1_DC_SETS;
extern unsigned int L1_DC_ASSOC;
extern unsigned int L1_DC_LINE_SIZE;
extern unsigned int L1_DC_HIT_LATENCY;
extern unsigned int L1_DC_MISS_LATENCY;
extern unsigned int L1_DC_NUM_MHSRs;
extern unsigned int L1_DC_MISS_SRV_PORTS;
extern unsigned int L1_DC_MISS_SRV_LATENCY;

// L1 Instruction Cache.
extern unsigned int L1_IC_SETS;
extern unsigned int L1_IC_ASSOC;
extern unsigned int L1_IC_LINE_SIZE;
extern unsigned int L1_IC_HIT_LATENCY;
extern unsigned int L1_IC_MISS_LATENCY;
extern unsigned int L1_IC_NUM_MHSRs;
extern unsigned int L1_IC_MISS_SRV_PORTS;
extern unsigned int L1_IC_MISS_SRV_LATENCY;

// L2 Unified Cache.
extern bool         L2_PRESENT;
extern unsigned int L2_SETS;
extern unsigned int L2_ASSOC;
extern unsigned int L2_LINE_SIZE;  // 2^LINE_SIZE bytes per line
extern unsigned int L2_HIT_LATENCY;
extern unsigned int L2_MISS_LATENCY;
extern unsigned int L2_NUM_MHSRs; 
extern unsigned int L2_MISS_SRV_PORTS;
extern unsigned int L2_MISS_SRV_LATENCY;

// L3 Unified Cache.
extern bool         L3_PRESENT;
extern unsigned int L3_SETS;
extern unsigned int L3_ASSOC;
extern unsigned int L3_LINE_SIZE;  // 2^LINE_SIZE bytes per line
extern unsigned int L3_HIT_LATENCY;
extern unsigned int L3_MISS_LATENCY;
extern unsigned int L3_NUM_MHSRs; 
extern unsigned int L3_MISS_SRV_PORTS;
extern unsigned int L3_MISS_SRV_LATENCY;

// Branch prediction unit
extern bool AUTO_BQ_SIZE;
extern unsigned int BQ_SIZE;
extern unsigned int BTB_ENTRIES;
extern unsigned int BTB_ASSOC;
extern unsigned int RAS_SIZE;
extern unsigned int COND_BRANCH_PRED_PER_CYCLE;
extern unsigned int CBP_ALGORITHM;
extern unsigned int CBP_PC_LENGTH;
extern unsigned int CBP_BHR_LENGTH;
extern unsigned int IBP_PC_LENGTH;
extern unsigned int IBP_BHR_LENGTH;
extern bool ENABLE_TRACE_CACHE;

// Benchmark control.
extern bool logging_on;
extern int64_t logging_on_at;

extern bool use_stop_amt;
extern uint64_t stop_amt;

extern uint64_t phase_interval;
extern uint64_t verbose_phase_counters;

#endif //PARAMETERS_H
