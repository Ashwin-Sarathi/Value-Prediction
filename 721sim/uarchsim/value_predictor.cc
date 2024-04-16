#include "value_predictor.h"
#include <algorithm>
#include <cassert>
#include <iostream>

// vp::vp() {

// }

// void vp::vp_dump_stats(FILE* fp) {
//     fprintf(fp, "VPU MEASUREMENTS-----------------------------------\n");

//     fprintf(fp, "vpmeas_ineligible\t\t:\t%lu\n", vpmeas_ineligible);
//     fprintf(fp, "\tvpmeas_ineligible_type:\t%lu\n", vpmeas_ineligible_type);
//     fprintf(fp, "\tvpmeas_ineligible_drop:\t%lu\n\n", vpmeas_ineligible_drop);

//     fprintf(fp, "vpmeas_eligible\t\t:\t%lu\n", vpmeas_eligible);
//     fprintf(fp, "\tvpmeas_miss:\t%lu\n", vpmeas_miss);
//     fprintf(fp, "\tvpmeas_conf_corr:\t%lu\n", vpmeas_conf_corr);
//     fprintf(fp, "\tvpmeas_conf_incorr:\t%lu\n", vpmeas_conf_incorr);
//     fprintf(fp, "\tvpmeas_unconf_corr:\t%lu\n", vpmeas_unconf_corr);
//     fprintf(fp, "\tvpmeas_unconf_incorr:\t%lu\n", vpmeas_unconf_incorr);
// }