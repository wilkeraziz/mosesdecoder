#include <cstdlib>
#include <algorithm>
#include <vector>
#include <deque>
#include <set>
#include <fstream>
#include <iostream>
#include "moses/WordsBitmap.h"
#include "moses/WordsRange.h"
#include "moses/Hypothesis.h"
#include "moses/ScoreComponentCollection.h"
#include "moses/TargetPhrase.h"
#include "moses/InputType.h"
#include "moses/TranslationOption.h"
#include "moses/Util.h"
#include "moses/AlignmentInfo.h"
#include "moses/InputPath.h"
#include "moses/FF/PermutationDistortion.h"

using namespace std;

namespace  // HELPER stuff useful in this file only
{

// Computes the usual distortion cost function
int ComputeDistortionCost(const std::size_t left, const std::size_t right)
{
    int jump = right - left - 1;
    return (jump < 0)? -jump : jump;
}

// Computes distortion cost given a sequence of positions
int ComputeDistortionCost(const std::vector<std::size_t> &positions)
{
    if (positions.empty())
        return 0;
    int d = 0;
    int last_covered = positions.front();
    for (std::size_t i = 1; i < positions.size(); ++i) {
        d += ComputeDistortionCost(last_covered, positions[i]);
        last_covered = positions[i];
    }
    return d;
}

} 

namespace Moses
{

PermutationDistortion::PermutationDistortion(const std::string &line)
  :StatefulFeatureFunction(4, line), 
    m_unfold_heuristic('M'), 
    m_internal_scoring(true),
    m_wa_scoring(true),
    m_sstate_fname("index")
{
    ReadParameters();
    
    // sanity checks
    
    // update the number of components
    /*
    std::size_t n_components = 0;
    if (m_wa_scoring) {
        n_components += (m_internal_scoring)? 4 : 2;
    } else {
        n_components += (m_internal_scoring)? 2 : 1;
    } 
    m_numScoreComponents = n_components;  // not sure how we get to have the number of features depend on the parameters of the FF
    */
}

/**
 * CONFIG FF
 */

void PermutationDistortion::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "originalPosLabel" || key == "sstate-fname") {
        m_sstate_fname = value;
    } else if (key == "targetWordOrderHeuristic" || key == "unfold") {
      if (value == "none" || value == "monotone")
          m_unfold_heuristic = 'M';
      else if (value == "unalignedAttachesToLeft" || value == "left") 
          m_unfold_heuristic = 'L';
      else if (value == "unalignedAttachesToRight" || value == "right") 
          m_unfold_heuristic = 'R';
      else
          UTIL_THROW2("Unknown heuristic: " + value);
    } else if (key == "scorePermutationsWithinPhrases") {
        if (ToLower(value) == "no" || value == "0" || ToLower(value) == "false")
           m_internal_scoring = false; 
    } else if (key == "permuteUsingWordAlignments") {
        if (ToLower(value) == "no" || value == "0" || ToLower(value) == "false")
           m_wa_scoring = false; 
    } else {
        FeatureFunction::SetParameter(key, value);
    }
}


/**
 * COMPUTE FF
 */
  

void PermutationDistortion::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{
    if (!m_internal_scoring) // maybe we don't care about internal scoring
        return;

    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // score phrases wrt how they permute the input (this uses the lattice input and ignores word alignment)
    std::vector<std::size_t> positions(GetInputPositions(inputPath, m_sstate_fname)); 
    SetInternalDistortionCost(scores, ComputeDistortionCost(positions));

    if (m_wa_scoring) { // maybe we want to rely on word-alignment
        // score phrases wrt how they permute the input given word alignment information (this rearranges the lattice input in target word order)
        // unaligned words behave according to the option of unfold heuristic
        std::vector<std::size_t> permutation(GetPermutation(positions, targetPhrase.GetAlignTerm(), m_unfold_heuristic));
        SetInternalDistortionCostGivenWA(scores, ComputeDistortionCost(permutation));
    }

    // update feature vector 
    scoreBreakdown.PlusEquals(this, scores);
}


FFState* PermutationDistortion::EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
      ScoreComponentCollection* accumulator) const
{
    // here we need to score the hypothesis wrt yet untranslated words
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // first we need to get information from the input path that the current phrase covers
    const TranslationOption& topt = hypo.GetTranslationOption();
    const InputPath& path = topt.GetInputPath(); 
    // from the arcs of the lattice we get an input permutation
    std::vector<std::size_t> positions = GetInputPositions(path, m_sstate_fname);
    
    // now we can update the current coverage vector
    const PermutationDistortionState* prev = dynamic_cast<const PermutationDistortionState*>(prev_state);  // previous coverage

    // compute distortion cost external to the phrase assuming the input permutation (that of the lattice)
    SetExternalDistortionCost(scores, ComputeDistortionCost(prev->GetLastCovered(), positions.front()));
    if (m_wa_scoring) {
        // here we rearrange the words in target word order via word alignment information 
        std::vector<std::size_t> permutation(GetPermutation(positions, topt.GetTargetPhrase().GetAlignTerm(), m_unfold_heuristic));
        // compute disttortion cost external to the phrase assuming target word order (by rearranging lattice input using word alignment information) 
        SetExternalDistortionCostGivenWA(scores, ComputeDistortionCost(prev->GetLastCoveredGivenWA(), permutation.front()));
        
        accumulator->PlusEquals(this, scores);
        return new PermutationDistortionState(positions.back(), permutation.back());
    } else {
        accumulator->PlusEquals(this, scores);
        return new PermutationDistortionState(positions.back(), -1);
    }
}



}


