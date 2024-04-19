#include "pipeline.h"
#include <iostream>
using namespace std;

//Has been fixed 

////////////////////////////////////////////////////////////////////////////////////
// The Rename Stage has two sub-stages:
// rename1: Get the next rename bundle from the FQ.
// rename2: Rename the current rename bundle.
////////////////////////////////////////////////////////////////////////////////////

void pipeline_t::rename1() {
   unsigned int i;
   unsigned int rename1_bundle_width;

   ////////////////////////////////////////////////////////////////////////////////////
   // Try to get the next rename bundle.
   // Two conditions might prevent getting the next rename bundle, either:
   // (1) The current rename bundle is stalled in rename2.
   // (2) The FQ does not have enough instructions for a full rename bundle,
   //     and it's not because the fetch unit is stalled waiting for a
   //     serializing instruction to retire (fetch exception, amo, or csr instruction).
   ////////////////////////////////////////////////////////////////////////////////////

   // Check the first condition. Is the current rename bundle stalled, preventing
   // insertion of the next rename bundle? Check whether or not the pipeline register
   // between rename1 and rename2 still has a rename bundle.

   if (RENAME2[0].valid) {	// The current rename bundle is stalled.
      return;
   }

   // Check the second condition.
   // Stall if the fetch unit is active (it's not waiting for a serializing
   // instruction to retire) and the FQ doesn't have enough instructions for a full
   // rename bundle.

   rename1_bundle_width = ((FQ.get_length() < dispatch_width) ? FQ.get_length() : dispatch_width);

   if (FetchUnit->active() && (rename1_bundle_width < dispatch_width)) {
      return;
   }

   // Get the next rename bundle.
   for (i = 0; i < rename1_bundle_width; i++) {
      assert(!RENAME2[i].valid);
      RENAME2[i].valid = true;
      RENAME2[i].index = FQ.pop();
   }
}

void pipeline_t::rename2() {
   unsigned int i;
   unsigned int index;
   unsigned int bundle_dst, bundle_branch, bundle_VPQ;
   bool not_enough_vpq;

   // Stall the rename2 sub-stage if either:
   // (1) There isn't a current rename bundle.
   // (2) The Dispatch Stage is stalled.
   // (3) There aren't enough rename resources for the current rename bundle.

   if (!RENAME2[0].valid ||	// First stall condition: There isn't a current rename bundle.
       DISPATCH[0].valid) {	// Second stall condition: The Dispatch Stage is stalled.
      return;
   }


   // This is the first place where VPU flags get set, so it would be wise to initialize all the flags here
   for (i = 0; i < dispatch_width; i++) {
      if (!RENAME2[i].valid)
         break;			// Not a valid instruction: Reached the end of the rename bundle so exit loop.

      index = RENAME2[i].index;

      PAY.buf[index].vp_ineligible        = false;
      PAY.buf[index].vp_ineligible_type   = false;
      PAY.buf[index].vp_ineligible_drop   = false;

      PAY.buf[index].vp_eligible          = false;
      PAY.buf[index].vp_miss              = false;
      PAY.buf[index].vp_confident         = false;
      PAY.buf[index].vp_unconfident       = false;
      PAY.buf[index].vp_correct           = false;
      PAY.buf[index].vp_incorrect         = false;
   }

   // Third stall condition: There aren't enough rename resources for the current rename bundle.
   bundle_dst = 0;
   bundle_branch = 0;
   bundle_VPQ = 0;
   for (i = 0; i < dispatch_width; i++) {
      if (!RENAME2[i].valid)
         break;			// Not a valid instruction: Reached the end of the rename bundle so exit loop.

      index = RENAME2[i].index;

      
      // FIX_ME #1
      // Count the number of instructions in the rename bundle that need a checkpoint (most branches).
      // Count the number of instructions in the rename bundle that have a destination register.
      // With these counts, you will be able to query the renamer for resource availability
      // (checkpoints and physical registers).
      //
      // Tips:
      // 1. The loop construct, for iterating through all instructions in the rename bundle (0 to dispatch_width),
      //    is already provided for you, above. Note that this comment is within the loop.
      // 2. At this point of the code, 'index' is the instruction's index into PAY.buf[] (payload).
      // 3. The instruction's payload has all the information you need to count resource needs.
      //    There is a flag in the instruction's payload that *directly* tells you if this instruction needs a checkpoint.
      //    Another field indicates whether or not the instruction has a destination register.
      
      //********************************************
      // FIX_ME #1 BEGIN
      //********************************************

      //Check for the existence of a destination register and checkpoints 
      if(PAY.buf[index].C_valid){
         bundle_dst++;
      }
      if(PAY.buf[index].checkpoint){
         bundle_branch++; 
      }

      ////////////////////////////////////////////////////////////////////////////////////////////////////
      // Checking the eligibility of instructions for value prediction
      // 1. Checks if its oracle confidence mode. If it is then on top of other conditions, also checks 
      //    if it is a good instruction. If yes, eligible, else not eligible.
      // 2. If it is real confidence, then just checks base conditions.
      ////////////////////////////////////////////////////////////////////////////////////////////////////

      bool load_flag = IS_LOAD(PAY.buf[index].flags); // If true, then instruction is of type load

      if (VALUE_PREDICTION_ENABLED && !PERFECT_VALUE_PREDICTION) {
         if (oracle_confidence) {
            if (VPU.isEligible(PAY.buf[index].pc, PAY.buf[index].checkpoint, PAY.buf[index].C_valid, PAY.buf[index].fu, load_flag) && PAY.buf[index].good_instruction) {
               bundle_VPQ ++;
               PAY.buf[index].vp_eligible = true;
            }
            else {
               PAY.buf[index].vp_eligible = false;
               PAY.buf[index].vp_ineligible_type = true;
            }
         }
         else {
            if(VPU.isEligible(PAY.buf[index].pc, PAY.buf[index].checkpoint, PAY.buf[index].C_valid, PAY.buf[index].fu, load_flag)){
               bundle_VPQ++;
               PAY.buf[index].vp_eligible = true;
            }   
            else {
               PAY.buf[index].vp_eligible = false;
               PAY.buf[index].vp_ineligible_type = true;
            }
         }
      }

      // Sanity check to ensure that if VP mode is off, then an instruction is never eligible for prediction.
      if (!VALUE_PREDICTION_ENABLED) {
         assert(!PAY.buf[index].vp_eligible);
      }

      //********************************************
      // FIX_ME #1 END
      //********************************************
   }

   // FIX_ME #2
   // Check if the Rename2 Stage must stall due to any of the following conditions:
   // * Not enough free checkpoints.
   // * Not enough free physical registers.
   //
   // If there are not enough resources for the *whole* rename bundle, then stall the Rename2 Stage.
   // Stalling is achieved by returning from this function ('return').
   // If there are enough resources for the *whole* rename bundle, then do not stall the Rename2 Stage.
   // This is achieved by doing nothing and proceeding to the next statements.

   //********************************************
   // FIX_ME #2 BEGIN
   //********************************************

   //calling the stall branch & stall_reg functions from the renamer class
   //REN ---> register rename module
   if(REN->stall_branch(bundle_branch)) {
         return;
   }
   if(REN->stall_reg(bundle_dst)) {
         return;
   }

   if (VALUE_PREDICTION_ENABLED) {
      not_enough_vpq = VPU.stallVPQ(bundle_VPQ);
      if(not_enough_vpq && !vpq_full_policy) {
         return;
      }
   }

   //********************************************
   // FIX_ME #2 END
   //********************************************

   //
   // Sufficient resources are available to rename the rename bundle.
   //
   for (i = 0; i < dispatch_width; i++) {
      if (!RENAME2[i].valid)
         break;			// Not a valid instruction: Reached the end of the rename bundle so exit loop.

      index = RENAME2[i].index;

      // FIX_ME #3
      // Rename source registers (first) and destination register (second).
      //
      // Tips:
      // 1. At this point of the code, 'index' is the instruction's index into PAY.buf[] (payload).
      // 2. The instruction's payload has all the information you need to rename registers, if they exist. In particular:
      //    * whether or not the instruction has a first source register, and its logical register number
      //    * whether or not the instruction has a second source register, and its logical register number
      //    * whether or not the instruction has a third source register, and its logical register number
      //    * whether or not the instruction has a destination register, and its logical register number
      // 3. When you rename a logical register to a physical register, remember to *update* the instruction's payload with the physical register specifier,
      //    so that the physical register specifier can be used in subsequent pipeline stages.

      //********************************************
      // FIX_ME #3 BEGIN
      //********************************************

      //The source rgisters are A, B, D. Defined in payload.h
      //Valid determines the existence of the register
      //If true call the rename_rsrc function from the renamer class. Input: log_reg, the logical register to rename
      //log_reg is the specifier of the source register 
      if (PAY.buf[index].A_valid){
        PAY.buf[index].A_phys_reg = REN->rename_rsrc(PAY.buf[index].A_log_reg);
      }
      if (PAY.buf[index].B_valid){
        PAY.buf[index].B_phys_reg = REN->rename_rsrc(PAY.buf[index].B_log_reg);
      }
      if (PAY.buf[index].D_valid){
        PAY.buf[index].D_phys_reg = REN->rename_rsrc(PAY.buf[index].D_log_reg);
      }

      //The destination register is C
      //If valid, call rename_rdst function. Input: log_reg, the logical register to rename      
      if (PAY.buf[index].C_valid) {
         PAY.buf[index].C_phys_reg = REN->rename_rdst(PAY.buf[index].C_log_reg);
      }

      ////////////////////////////////////////////////////////////////////////////////////////////
      // Predicting values of destination registers 
      // 1. If an instruction is eligible for value prediction, it is first added to the VPQ
      // 2. Then we have 2 modes to look for a prediction, oracle and real:
      //    a. Oracle: We try to obtain a prediction and this prediction is passed only if 
      //       it is correct, and hence it is confident.
      //    b. Real: We obtain a prediction and it is passed regardless. Only difference is that,
      //       in case of real, the prediction is written to PRF only if it is confident. But we
      //       must always record the prediction to check for lost opportunity.
      ////////////////////////////////////////////////////////////////////////////////////////////

      // We predict only for real and oracle modes here since perfect value prediction is handled in dispatch
      if (VALUE_PREDICTION_ENABLED && !PERFECT_VALUE_PREDICTION && PAY.buf[index].vp_eligible) {
         // Confirms that an instruction that makes it here matches the predefined eligibility criteria
         if (oracle_confidence) {
            assert(VPU.isEligible(PAY.buf[index].pc, PAY.buf[index].checkpoint, PAY.buf[index].C_valid, PAY.buf[index].fu, IS_LOAD(PAY.buf[index].flags)) && PAY.buf[index].good_instruction);
         } 
         else {
            assert(VPU.isEligible(PAY.buf[index].pc, PAY.buf[index].checkpoint, PAY.buf[index].C_valid, PAY.buf[index].fu, IS_LOAD(PAY.buf[index].flags)));
         }

         // If VPQ full and need entries for VP-eligible instructions in bundle => (vpq_full_policy == 0): stall bundle, (vpq_full_policy == 1): don’t allocate VPQ entries
         // If vpq_full_policy is 1 and VPU is full, don't allocate
         if (VPU.isVPQFull()) {
            if (vpq_full_policy) {
               PAY.buf[index].vp_eligible = false;
               PAY.buf[index].vp_ineligible_drop = true;
            }
            else {
               cerr << "VPQ can't be full if vpq_full_policy is stall." << endl;
               assert(1);
            }
         }
         else {
            PAY.buf[index].vpq_index = VPU.enqueue(PAY.buf[index].pc);
         }

         uint64_t predicted_value;
         db_t* actual_value;

         // When in oracle mode, prediction is fed only if is correct, else it is always unconfident and incorrect
         if (oracle_confidence) {            
               actual_value = get_pipe()->peek(PAY.buf[index].db_index);
               int prediction_result = VPU.getOraclePrediction(PAY.buf[index].pc, predicted_value, actual_value->a_rdst[0].value);

               // Tag match but value mismatch
               if(prediction_result == 1){
                  PAY.buf[index].predicted_value = predicted_value;
                  PAY.buf[index].vp_unconfident = true; 
                  PAY.buf[index].vp_incorrect = true;
               }

               // Tag match and value match case
               else if (prediction_result == 2) {
                  PAY.buf[index].predicted_value = predicted_value;
                  PAY.buf[index].vp_confident = true; 
                  PAY.buf[index].vp_correct = true;
               }

               // Tag not found
               else if (prediction_result == 0) {
                  PAY.buf[index].vp_miss = true;
               }

               // This case is not possible
               else {
                  assert(1);
               }
         }

         // When in real confidence mode, prediction is fed regardless and is checked later for correctness
         else {            
            int prediction_result = VPU.getRealPrediction(PAY.buf[index].pc, predicted_value);

            // Tag match and max confidence
            if (prediction_result == 2) {
               PAY.buf[index].predicted_value = predicted_value; // Update the predicted value in the payload
               PAY.buf[index].vp_confident = true;
            }

            // Tag match but not max condifence
            else if (prediction_result == 1) {
               PAY.buf[index].predicted_value = predicted_value;
               PAY.buf[index].vp_unconfident = true;
            }

            // Tag not found
            else if (prediction_result == 0) {
               PAY.buf[index].vp_miss = true;
            }

            // This case is not possible
            else {
               assert(1);
            }            
         }
      }

      //********************************************
      // FIX_ME #3 END
      //********************************************

      // FIX_ME #4
      // Get the instruction's branch mask.
      //
      // Tips:
      // 1. Every instruction gets a branch_mask. An instruction needs to know which branches it depends on, for possible squashing.
      // 2. The branch_mask is not held in the instruction's PAY.buf[] entry. Rather, it explicitly moves with the instruction
      //    from one pipeline stage to the next. Normally the branch_mask would be wires at this point in the logic but since we
      //    don't have wires place it temporarily in the RENAME2[] pipeline register alongside the instruction, until it advances
      //    to the DISPATCH[] pipeline register. The required left-hand side of the assignment statement is already provided for you below:
      //    RENAME2[i].branch_mask = ??;

      //********************************************
      // FIX_ME #4 BEGIN
      //********************************************

      //branch mask function has been defined in the renamer class to get the branch mask for an instruction
      RENAME2[i].branch_mask = REN->get_branch_mask(); 

      //********************************************
      // FIX_ME #4 END
      //********************************************

      // FIX_ME #5
      // If this instruction requires a checkpoint (most branches), then create a checkpoint.
      //
      // Tips:
      // 1. At this point of the code, 'index' is the instruction's index into PAY.buf[] (payload).
      // 2. There is a flag in the instruction's payload that *directly* tells you if this instruction needs a checkpoint.
      // 3. If you create a checkpoint, remember to *update* the instruction's payload with its branch ID
      //    so that the branch ID can be used in subsequent pipeline stages.
      
      //********************************************
      // FIX_ME #5 BEGIN
      //********************************************

      //The flag that tells if a checkpoint is needed is the 'checkpoint'
      //the checkpoint function in the renamer class creates a new checkpoint for a branch 
      //The output of the function is the branch's ID
      //Store the ID 
      if (PAY.buf[index].checkpoint){
         uint64_t vpq_tail1 = 0;
         bool vpq_tail_phase_bit1 = 0;
         VPU.getTailForCheckpoint(vpq_tail1, vpq_tail_phase_bit1);
         PAY.buf[index].branch_ID = REN->checkpoint(vpq_tail1, vpq_tail_phase_bit1);
      }

      //********************************************
      // FIX_ME #5 END
      //********************************************
   }

   //
   // Transfer the rename bundle from the Rename Stage to the Dispatch Stage.
   //
   for (i = 0; i < dispatch_width; i++) {
      if (!RENAME2[i].valid)
         break;			// Not a valid instruction: Reached the end of the rename bundle so exit loop.

      assert(!DISPATCH[i].valid);
      RENAME2[i].valid = false;
      DISPATCH[i].valid = true;
      DISPATCH[i].index = RENAME2[i].index;
      DISPATCH[i].branch_mask = RENAME2[i].branch_mask;
   }
}