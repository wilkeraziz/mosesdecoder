#pragma once

#include <vector>
#include <map>
#include <string>
#include "moses/WordsRange.h"
#include "moses/FF/StatefulFeatureFunction.h"
#include "moses/Hypothesis.h"
#include "moses/ChartHypothesis.h"
#include "moses/FF/FFState.h"
#include "moses/TypeDef.h"

namespace Moses
{

/*
 * This feature computes distortion penalty correctly when the input is a lattice of permutations.
 * The lattice must be annotated with the original positions corresponding to each arc (word).
 *
 * Arguments:
 *  originalPosLabel=<string>
 *      this is the name of the feature labelling PLF arcs which stores the original position associated with the input words labelling the arc
 *  unfold=monotone|left|right
 *      to permute input paths into target word-order we use alignment information within phrase pairs
 *      when doing so we have a heuristic to deal with unaligned words, 
 *      we can attach them to the left or to the right;
 *      if you don't want to compute such permutations, you can use the option 'monotone' which will simply preserve the input path's word order
 *
 *  Features:
 *   1. distortion cost
 *      The usual notion of distortion cost, however, the positions associated with boundary words of phrases are translated back to original word order
 *      by using the permutations provided in lattice
 *   2. distortion cost internal to phrases
 *      Note that with preordered input, a contiguous source phrase may cover a discontiguous sequence of source words (wrt the original word order).
 *      This feature captures how much distortion we observe within phrases.
 *   3. same as (2) but wrt the input path permuted to target word-order using word alignment information
 *
 */    
class LatticeDistortionPenaltyState : public FFState
{
public:
    LatticeDistortionPenaltyState(const int last_covered,
            const int last_covered_given_wa): 
        m_last_covered(last_covered),
        m_last_covered_given_wa(last_covered_given_wa) 
    {}

    inline const int GetLastCovered() const
    {
        return m_last_covered;
    }

    inline const int GetLastCoveredGivenWA() const
    {
        return m_last_covered_given_wa;
    }

    inline int Compare(const FFState& other) const
    {
        const LatticeDistortionPenaltyState& rhs = dynamic_cast<const LatticeDistortionPenaltyState&>(other);
        if (m_last_covered == rhs.GetLastCovered()) 
            return (m_last_covered_given_wa < rhs.GetLastCoveredGivenWA())? -1 : 1;
        else 
            return (m_last_covered < rhs.GetLastCovered())? -1 : 1;
    }
    
private:
    const int m_last_covered;
    const int m_last_covered_given_wa;
};


class LatticeDistortionPenalty : public StatefulFeatureFunction
{
private:
    //std::size_t m_seg; // segment being translated (necessary in referring to table of permutations and expectations)
    // selects a heuristic to unfold word alignments, that is,  to permute the input in target word-order)
    // this heuristic accepts 3 values:
    //  'M' (for montone) means skip it altogether, that is, ignore word alignment information and assume a monotone mapping between input and output
    //  'L' (for left) means consult word alignment information to obtain target word-order and attaches unaligned words to its left
    //  'R' (for right) similar to 'L', but looks for aligned words to the right of the unaligned ones
    char m_unfold_heuristic; 
    bool m_internal_scoring;
    bool m_wa_scoring;
    std::string m_sstate_fname;  // name of arc feature representing the state in s (original source)

public:
  LatticeDistortionPenalty(const std::string &line);

  bool IsUseable(const FactorMask &mask) const {
    return true;
  }
  
  void Load()
  {
  }
  
  inline const FFState* EmptyHypothesisState(const InputType &input) const
  {
      return new LatticeDistortionPenaltyState(-1, -1);
  }
  
  // we have nothing to do here
  void EvaluateInIsolation(const Phrase &source
                           , const TargetPhrase &targetPhrase
                           , ScoreComponentCollection &scoreBreakdown
                           , ScoreComponentCollection &estimatedFutureScore) const
  {
  }
  
  // we do not need to score oovs, because they do not contribute towards higher Kendall Tau
  void EvaluateInIsolation(const InputPath &inputPath
                           , const TargetPhrase &targetPhrase
                           , ScoreComponentCollection &scoreBreakdown
                           , ScoreComponentCollection &estimatedFutureScore) const
  {
  }

  // we have nothing to do here
  void EvaluateTranslationOptionListWithSourceContext(const InputType &input
      , const TranslationOptionList &translationOptionList) const
  {
  }

  
  /*
   * At this point we know the phrase will cover a certain range of the input.
   * Even though we do not have global information about the permutation (wrt target word order),
   * that is, we do not know the when this phrase will be applied (i.e. the segmentation),
   * we can still score the phrase as a function of how its words are internally permuted.
  */
  void EvaluateWithSourceContext(const InputType &input
                                 , const InputPath &inputPath
                                 , const TargetPhrase &targetPhrase
                                 , const StackVec *stackVec
                                 , ScoreComponentCollection &scoreBreakdown
                                 , ScoreComponentCollection *estimatedFutureScore = NULL) const;

  FFState* EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
          ScoreComponentCollection* accumulator) const;

  FFState* EvaluateWhenApplied(const ChartHypothesis& hypo,
          int featureID,
          ScoreComponentCollection* accumulator) const {
      UTIL_THROW2("LatticeDistortionPenalty does not support chart-based decoding.")
  }

  void SetParameter(const std::string& key, const std::string& value);
  
  void InitializeForInput(InputType const& source)
  {
        UTIL_THROW_IF2(source.GetType() != ConfusionNetworkInput,  // Moses can be so ugly some times :( how can a WordLattice be a type of ConfusionNetwork and not the other way around?
            "LatticeDistortionPenalty only supports lattice input (for sentence input see PreorderedDistortionPenalty)");
  }

private:
    
    /*
     * This method abstracts away the position of the feature "External Distortion Cost" in the vector of score components.
     * Distortion cost is a function of the distance between the FIRST word in the current phrase
     * and the LAST word of the previous phrase.
     *
     * In this version we IGNORE word alignment information, thus both the previous and the current phrases are assumed 
     * to be (or have been) covered in the order given by the lattice.
     */
    inline void SetExternalDistortionCost(std::vector<float> &scores, const float score) const
    {
        scores[0] = score;
    }
    
    /*
     * This method abstracts away the position of the feature "External Distortion Cost Given Word Alignment" in the vector of score components.
     * Distortion cost is a function of the distance between the FIRST word in the current phrase
     * and the LAST word of the previous phrase.
     *
     * In this version we USE word alignment information, thus both the previous and the current phrases are assumed 
     * to be (or have been) covered in target word order.
     */
    inline void SetExternalDistortionCostGivenWA(std::vector<float> &scores, const float score) const
    {
        scores[1] = score;
    }
    
    /*
     * This method abstracts away the position of the feature "Internal Distortion Cost" in the vector of score components.
     * We iterate through the phrase from left-to-right accumulating the absolute values of the jumps.
     *
     * In this version we IGNORE word alignment information, thus the phrases is assumed 
     * to be covered in the order given by the lattice.
     */
    inline void SetInternalDistortionCost(std::vector<float> &scores, const float score) const
    {
        scores[2] = score;
    }
    
    /*
     * This method abstracts away the position of the feature "Internal Distortion Cost Given Word Alignment" in the vector of score components.
     * We iterate through the phrase from left-to-right accumulating the absolute values of the jumps.
     *
     * In this version we USE word alignment information, thus the phrase is assumed 
     * to be covered in target word order.
     */
    inline void SetInternalDistortionCostGivenWA(std::vector<float> &scores, const float score) const
    {
        scores[3] = score;
    }
};

}


