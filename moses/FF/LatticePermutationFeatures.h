#pragma once

#include <vector>
#include <string>
#include "moses/InputPath.h"
#include "moses/AlignmentInfo.h"

namespace Moses
{
    
/*
 * This retrieves the permutation (a vector of 0-based indices) associated with a certain input path.
 * We retrieve these indices by inspecting the value of a named-feature stored in each arc of the lattice.
 */
std::vector<std::size_t> GetInputPositions(const InputPath &inputPath, const std::string& key);

// This permutes elements given in source language word order into target language word order.
// Please select a heuristic to unfold word alignments, that is, to permute the input in target word-order.
// This heuristic accepts 3 values:
//  'M' (for montone) means skip it altogether, that is, ignore word alignment information and assume a monotone mapping between input and output
//  'L' (for left) means consult word alignment information to obtain target word-order and attaches unaligned words to its left
//  'R' (for right) similar to 'L', but looks for aligned words to the right of the unaligned ones
std::vector<std::size_t> GetPermutation(const std::vector<std::size_t> &input, const AlignmentInfo& ainfo, const char targetWordOrderHeuristic);


} // namespace Moses
