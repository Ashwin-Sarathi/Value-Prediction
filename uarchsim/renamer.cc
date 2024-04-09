#include "renamer.h"
#include <cassert>

/////////////////////////////////////////////////////////////////////
// Constructor Function
/////////////////////////////////////////////////////////////////////

renamer::renamer(uint64_t n_log_regs, uint64_t n_phys_regs, uint64_t n_branches, uint64_t n_active) {
    this->totalLogicalRegisters     = n_log_regs;
    this->totalPhysicalRegisters    = n_phys_regs;
    this->numberOfCheckpoints       = n_branches;
    this->activeListSize            = n_active;
    this->freeListSize              = n_phys_regs - n_log_regs;
            
    assert(n_phys_regs > n_log_regs);
    assert((n_branches >= 1) && (n_branches <= 64));
    assert(n_active > 0);

    RMT                     =          new uint64_t[n_log_regs];
    AMT                     =          new uint64_t[n_log_regs];
    freeList                =        new uint64_t[freeListSize];
    AL                      =          new activeList[n_active];
    PRF                     =         new uint64_t[n_phys_regs];
    readyBitArray           =             new bool[n_phys_regs];
    stateCheckpoint         =  new branchCheckpoint[n_branches];

    for (int i = 0; i < n_branches; i++) {            
        stateCheckpoint[i].shadowMapTable = new uint64_t[n_log_regs];
    }

    initialize();
}

void renamer::initialize() {
    // Initializing RMT and AMT values
    for (int i = 0; i < totalLogicalRegisters; i++) {
        RMT[i] = i;
        AMT[i] = i;
    }

    // Initializing the free List
    freeListHead      = 0;
    freeListTail      = 0;
    freeListHeadPhase = 0;
    freeListTailPhase = 1;
    for (int i = 0; i < freeListSize; i++) {
        freeList[i] = totalLogicalRegisters + i;
    }

    // Initializing the Active List
    initializeActiveList();

    // Initializing the PRF and Ready bit array
    for (int i = 0; i < totalPhysicalRegisters; i++) {
        //PRF[i]              =     0;
        readyBitArray[i]    =  true;
    }

    // Initializing the Global Branch Mask
    GBM = 0;

    // // Intializing the state checkpoints
    // for (int i = 0; i < numberOfCheckpoints; i++) {
    //     for (int j = 0; j < totalLogicalRegisters; j++) {
    //         stateCheckpoint[i].shadowMapTable[j] =      i;
    //     }
    //     stateCheckpoint[i].freeListHead          =       0;
    //     stateCheckpoint[i].freeListHeadPhase     =   false;
    //     stateCheckpoint[i].checkpointedGBM       =       0;
    // }
}

void renamer::initializeActiveList() {        
    activeListHead = 0;
    activeListTail = 0;
    activeListHeadPhase = 0;
    activeListTailPhase = 0;
    for (int i = 0; i < activeListSize; i++) {
        AL[i].destFlag                      =   false;
        AL[i].destLogicalRegisterNumber     =  UINT64_MAX;
        AL[i].destPhysicalRegisterNumber    =   UINT64_MAX;
        AL[i].complete                      =   false;
        AL[i].exception                     =   false;
        AL[i].loadViolation                 =   false;
        AL[i].branchMisprediction           =   false;
        AL[i].valueMisprediction            =   false;
        AL[i].loadFlag                      =   false;
        AL[i].storeFlag                     =   false;
        AL[i].branchFlag                    =   false;
        AL[i].amoFlag                       =   false;
        AL[i].csrFlag                       =   false;
        AL[i].pc                            =  UINT64_MAX;
    }
}

// void renamer::printPRF() {
//     cout << "PRINTING PRF READY BIT ARRAY" << endl; 
//     for (int i = 0; i < 60; i++) {
//         cout << i << ": " << readyBitArray[i] << endl;
//     }
//     cout << sizeof( bool);
// }

/////////////////////////////////////////////////////////////////////
// Private functions.
// e.g., a generic function to copy state from one map to another.
/////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////
// Functions regarding the Free List
/////////////////////////////////////////////////////////////////////

int renamer::freeListAvailability() {
    if (isFreeListEmpty()) {
        return 0;
    }
    if (isFreeListFull()) {
        return freeListSize;
    }

    int available = UINT64_MAX;
    if ((freeListHeadPhase != freeListTailPhase)) {
        assert(freeListHead > freeListTail);
        available = freeListTail - freeListHead + freeListSize;
        return available;
    }
    if ((freeListHeadPhase == freeListTailPhase)) {
        assert(freeListHead < freeListTail);
        available = freeListTail - freeListHead;
        return available;
    }
    return available;
}

bool renamer::isFreeListEmpty() {
    if ((freeListHeadPhase == freeListTailPhase) && (freeListHead == freeListTail)) {
        return true;
    }
    return false;
}

bool renamer::isFreeListFull() {
    if ((freeListHeadPhase != freeListTailPhase) && (freeListHead == freeListTail)) {
        return true;
    }
    return false;
}

void renamer::pushOntoFreeList(int phyDest) {
    assert(!isFreeListFull());
    freeList[freeListTail] = phyDest;

    if (freeListTail == (freeListSize - 1)) {
        freeListTail        = 0;
        freeListTailPhase   = !freeListTailPhase;
    }
    else {
        freeListTail ++;
    }
}

void renamer::restoreFreeList() {
    freeListHead       = freeListTail;
    freeListHeadPhase  = !freeListTailPhase;
}

/////////////////////////////////////////////////////////////////////
// Functions regarding the Active List
/////////////////////////////////////////////////////////////////////

int renamer::activeListAvailability() {
    if (isActiveListEmpty()) {
        return activeListSize;
    }
    if (isActiveListFull()) {
        return 0;
    }

    int available = UINT64_MAX;
    if (activeListHeadPhase != activeListTailPhase) {
        assert(activeListHead > activeListTail);
        available = activeListHead - activeListTail;
        return available;
    }
    if (activeListHeadPhase == activeListTailPhase) {
        assert(activeListHead < activeListTail); 
        available = activeListHead - activeListTail + activeListSize;
        return available;
    }
    
    return available;
}

bool renamer::isActiveListEmpty() {
    if ((activeListHead == activeListTail) && (activeListHeadPhase == activeListTailPhase)) {
        return true;
    }
    return false;
}

bool renamer::isActiveListFull() {
    if ((activeListHead == activeListTail) && (activeListHeadPhase != activeListTailPhase)) {
        return true;
    }
    return false;
}

int renamer::obtainActiveListIndex() {
    int index = activeListTail;

    assert(!isActiveListFull());

    if (activeListTail == (activeListSize - 1)) {
        activeListTail      = 0;
        activeListTailPhase = !activeListTailPhase;
    }
    else {
        activeListTail ++;
    }

    return index;
}

void renamer::retireFromActiveList() {
    assert(!isActiveListEmpty());

    if (activeListHead == (activeListSize - 1)) {
        activeListHead      = 0;
        activeListHeadPhase = !activeListHeadPhase;
    }
    else {
        activeListHead ++;
    }
}

void renamer::flushActiveList() {
    activeListTail      = activeListHead;
    activeListTailPhase = activeListHeadPhase;
    initializeActiveList();
}

////////////////////////////////////////
// Public functions.
////////////////////////////////////////

//////////////////////////////////////////
// Functions related to Rename Stage.   
//////////////////////////////////////////

bool renamer::stall_reg(uint64_t bundle_dst) {
    uint64_t available = freeListAvailability();
    assert(available != UINT64_MAX);

    if (available < bundle_dst) {
        return true;
    }
    else {
        return false;
    }
}

bool renamer::stall_branch(uint64_t bundle_branch) {
    // if (GBM == UINT64_MAX) return true;

    int freeCheckpoints = 0;
    int GBMChecker = 1;

    for (int i = 0; i < numberOfCheckpoints; i++) {
        if (GBM & GBMChecker) {
            GBMChecker <<= 1;
        }
        else {
            GBMChecker <<=1;
            freeCheckpoints ++;
        }
    }

    if (freeCheckpoints < bundle_branch) {
        return true;
    }
    return false;
}

uint64_t renamer::get_branch_mask() {
    return GBM;
}

uint64_t renamer::rename_rsrc(uint64_t log_reg) {
    return RMT[log_reg];
}

uint64_t renamer::rename_rdst(uint64_t log_reg) {
    assert(!isFreeListEmpty());
    
    RMT[log_reg] = freeList[freeListHead];

    if (freeListHead == (freeListSize - 1)) {
        freeListHead        = 0;
        freeListHeadPhase   = !freeListHeadPhase;
    }
    else {
        freeListHead ++;
    }

    return RMT[log_reg];
}

uint64_t renamer::checkpoint() {
    uint64_t bitMask = 1;
    int ID;

    assert(GBM != UINT64_MAX);

    for (int i = 0; i < numberOfCheckpoints; i++) {
        if (!(GBM & bitMask)) {
            ID = i;
            break;
        }
        else {
            bitMask <<= 1;
        }
    }
    GBM |= (1ULL << ID);

    stateCheckpoint[ID].checkpointedGBM     = GBM;
    stateCheckpoint[ID].freeListHead        = freeListHead;
    stateCheckpoint[ID].freeListHeadPhase   = freeListHeadPhase;
    for (int i = 0; i < totalLogicalRegisters; i++) {
        stateCheckpoint[ID].shadowMapTable[i] = RMT[i];
    }

    return ID;
    return 0;
}

//////////////////////////////////////////
// Functions related to Dispatch Stage. //
//////////////////////////////////////////

bool renamer::stall_dispatch(uint64_t bundle_inst) {
    if (isActiveListFull()) {
        return true;
    }
    int available = activeListAvailability();
    assert(available != UINT64_MAX);

    if (available < bundle_inst) {
        return true;
    }
    return false;
}

uint64_t renamer::dispatch_inst(bool dest_valid, uint64_t log_reg, uint64_t phys_reg, bool load, bool store, bool branch, bool amo, bool csr, uint64_t PC) {
    int activeListIndex = obtainActiveListIndex();

    AL[activeListIndex].destFlag = dest_valid;
    if (AL[activeListIndex].destFlag) {
        AL[activeListIndex].destLogicalRegisterNumber   = log_reg;
        AL[activeListIndex].destPhysicalRegisterNumber  = phys_reg;
    }
    else {
        AL[activeListIndex].destLogicalRegisterNumber   = -1;
        AL[activeListIndex].destPhysicalRegisterNumber  = -1;        
    }
    AL[activeListIndex].complete            =  false;
    AL[activeListIndex].exception           =  false;
    AL[activeListIndex].loadViolation       =  false;
    AL[activeListIndex].branchMisprediction =  false;
    AL[activeListIndex].valueMisprediction  =  false;
    AL[activeListIndex].loadFlag            =   load;
    AL[activeListIndex].storeFlag           =  store;
    AL[activeListIndex].branchFlag          = branch;
    AL[activeListIndex].amoFlag             =    amo;
    AL[activeListIndex].csrFlag             =    csr;
    AL[activeListIndex].pc                  =     PC;

    return activeListIndex;
}

bool renamer::is_ready(uint64_t phys_reg) {
    return readyBitArray[phys_reg];
}

void renamer::clear_ready(uint64_t phys_reg) {
    readyBitArray[phys_reg] = false;
}

//////////////////////////////////////////
// Functions related to the Reg. Read   //
// and Execute Stages.                  //
//////////////////////////////////////////

uint64_t renamer::read(uint64_t phys_reg) {
    return PRF[phys_reg];
}

void renamer::set_ready(uint64_t phys_reg) {
    readyBitArray[phys_reg] = true;
}

//////////////////////////////////////////
// Functions related to Writeback Stage.//
//////////////////////////////////////////

void renamer::write(uint64_t phys_reg, uint64_t value) {
    PRF[phys_reg] = value;
}

void renamer::set_complete(uint64_t AL_index) {
    AL[AL_index].complete = true;
}

void renamer::resolve(uint64_t AL_index, uint64_t branch_ID, bool correct) {
    if (correct) {
        GBM &= ~(1ULL << branch_ID);            
        for (int i = 0; i < numberOfCheckpoints; i++) {
            stateCheckpoint[i].checkpointedGBM &= ~(1ULL << branch_ID);
        }
    }
    else {
        GBM                 = stateCheckpoint[branch_ID].checkpointedGBM;
        GBM                &= ~(1ULL << branch_ID);

        for (int i = 0; i < totalLogicalRegisters; i++) {
            RMT[i] = stateCheckpoint[branch_ID].shadowMapTable[i];
        }

        freeListHead        = stateCheckpoint[branch_ID].freeListHead;
        freeListHeadPhase   = stateCheckpoint[branch_ID].freeListHeadPhase;

        if (AL_index == (activeListSize - 1)) {
            activeListTail = 0;
        }
        else {
            activeListTail = AL_index + 1;
        }

        if (activeListTail > activeListHead) {
            activeListTailPhase = activeListHeadPhase;
        }
        else {
            activeListTailPhase = !activeListHeadPhase;
        }
    }
}

//////////////////////////////////////////
// Functions related to Retire Stage.   //
//////////////////////////////////////////

bool renamer::precommit(bool &completed, bool &exception, bool &load_viol, bool &br_misp, bool &val_misp, bool &load, bool &store, bool &branch, bool &amo, bool &csr, uint64_t &PC) {
    if (isActiveListEmpty()) {
        return false;
    }

    completed = AL[activeListHead].complete;
    exception = AL[activeListHead].exception;
    load_viol = AL[activeListHead].loadViolation;
    br_misp   = AL[activeListHead].branchMisprediction;
    val_misp  = AL[activeListHead].valueMisprediction;
    load      = AL[activeListHead].loadFlag;
    store     = AL[activeListHead].storeFlag;
    branch    = AL[activeListHead].branchFlag;
    amo       = AL[activeListHead].amoFlag;
    csr       = AL[activeListHead].csrFlag;
    PC        = AL[activeListHead].pc;

    return true;
}

void renamer::commit() {
    assert(!isActiveListEmpty());
    assert(AL[activeListHead].complete);
    assert(!AL[activeListHead].exception);
    assert(!AL[activeListHead].loadViolation);

    if (AL[activeListHead].destFlag) {
        int logDest = AL[activeListHead].destLogicalRegisterNumber;
        assert(!isFreeListFull());
        pushOntoFreeList(AMT[logDest]);
        AMT[logDest] = AL[activeListHead].destPhysicalRegisterNumber;
    }
    
    retireFromActiveList();

}

void renamer::squash() {
    flushActiveList();
    restoreFreeList();

    for (int i = 0; i < totalLogicalRegisters; i++) {
        RMT[i] = AMT[i];
    }

    GBM = 0;
    for (int i = 0; i < numberOfCheckpoints; i++) {
        stateCheckpoint[i].checkpointedGBM       = 0;
        stateCheckpoint[i].freeListHead          = 0;
        stateCheckpoint[i].freeListHeadPhase     = 0;
        for (int j = 0; j < totalLogicalRegisters; j++) {
            stateCheckpoint[i].shadowMapTable[j] = 0;
        }
    }
}

//////////////////////////////////////////
// Functions not tied to specific stage.//
//////////////////////////////////////////

void renamer::set_exception(uint64_t AL_index) {
    AL[AL_index].exception = true;
}

void renamer::set_load_violation(uint64_t AL_index) {
    AL[AL_index].loadViolation = true;
}

void renamer::set_branch_misprediction(uint64_t AL_index) {
    AL[AL_index].branchMisprediction = true;
}

void renamer::set_value_misprediction(uint64_t AL_index) {
    AL[AL_index].valueMisprediction = true;
}

bool renamer::get_exception(uint64_t AL_index) {
    return AL[AL_index].exception;
}