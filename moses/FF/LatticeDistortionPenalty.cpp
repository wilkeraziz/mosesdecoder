#include "moses/FF/LatticeDistortionPenalty.h"
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
#include "moses/FF/ReorderingHelper.h"

using namespace std;


namespace Moses
{

LatticeDistortionPenalty::LatticeDistortionPenalty(const std::string &line)
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

void LatticeDistortionPenalty::SetParameter(const std::string& key, const std::string& value)
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
  

void LatticeDistortionPenalty::EvaluateWithSourceContext(const InputType &input
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
    std::vector<std::size_t> positions(ReorderingHelper::GetInputPositionsFromArcs(inputPath, m_sstate_fname)); 
    SetInternalDistortionCost(scores, ReorderingHelper::ComputeDistortionCost(positions));

    if (m_wa_scoring) { // maybe we want to rely on word-alignment
        // score phrases wrt how they permute the input given word alignment information (this rearranges the lattice input in target word order)
        // unaligned words behave according to the option of unfold heuristic
        std::vector<std::size_t> permutation(ReorderingHelper::GetPermutation(positions, targetPhrase.GetAlignTerm(), m_unfold_heuristic));
        SetInternalDistortionCostGivenWA(scores, ReorderingHelper::ComputeDistortionCost(permutation));
    }

    // update feature vector 
    scoreBreakdown.PlusEquals(this, scores);
}


FFState* LatticeDistortionPenalty::EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
      ScoreComponentCollection* accumulator) const
{
    // here we need to score the hypothesis wrt yet untranslated words
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // first we need to get information from the input path that the current phrase covers
    const TranslationOption& topt = hypo.GetTranslationOption();
    const InputPath& path = topt.GetInputPath(); 
    // from the arcs of the lattice we get an input permutation
    std::vector<std::size_t> positions = ReorderingHelper::GetInputPositionsFromArcs(path, m_sstate_fname);
    
    // now we can update the current coverage vector
    const LatticeDistortionPenaltyState* prev = dynamic_cast<const LatticeDistortionPenaltyState*>(prev_state);  // previous coverage

    // compute distortion cost external to the phrase assuming the input permutation (that of the lattice)
    SetExternalDistortionCost(scores, ReorderingHelper::ComputeDistortionCost(prev->GetLastCovered(), positions.front()));
    if (m_wa_scoring) {
        // here we rearrange the words in target word order via word alignment information 
        std::vector<std::size_t> permutation(ReorderingHelper::GetPermutation(positions, topt.GetTargetPhrase().GetAlignTerm(), m_unfold_heuristic));
        // compute disttortion cost external to the phrase assuming target word order (by rearranging lattice input using word alignment information) 
        SetExternalDistortionCostGivenWA(scores, ReorderingHelper::ComputeDistortionCost(prev->GetLastCoveredGivenWA(), permutation.front()));
        
        accumulator->PlusEquals(this, scores);
        return new LatticeDistortionPenaltyState(positions.back(), permutation.back());
    } else {
        accumulator->PlusEquals(this, scores);
        return new LatticeDistortionPenaltyState(positions.back(), -1);
    }
}



}


