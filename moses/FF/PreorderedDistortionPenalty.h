#pragma once

#include <vector>
#include <string>
#include "moses/WordsRange.h"
#include "moses/FF/StatefulFeatureFunction.h"
#include "moses/Hypothesis.h"
#include "moses/ChartHypothesis.h"
#include "moses/FF/FFState.h"
#include "moses/FF/ReorderingHelper.h"

namespace Moses
{

/*
 * This feature computes distortion costs correctly when the input is preordered text.
 * It only supports sentence input (for lattice input check LatticeDistortionPenalty).
 *
 * Arguments:
 *  mappings=<path>
 *      we interpret this as a table of permutations
 *      1 permutation per line
 *      each permutation is a space-separated sequence of 0-based positions mapping s' (preordered input) to s (original input)
 *
 * Features:
 *   1. distortion cost
 *      The usual notion of distortion cost, however, the positions associated with boundary words of phrases are translated back to original word order
 *      by using the permutations provided in the table.
 *   2. distortion cost internal to phrases
 *      Note that with preordered input, a contiguous source phrase may cover a discontiguous sequence of source words (wrt the original word order).
 *      This feature captures how much distortion we observe within phrases.
 */    
class PreorderedDistortionPenaltyState : public FFState
{
public:
    PreorderedDistortionPenaltyState(const int last_covered,
            const int last_covered_given_wa = -1): 
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
        const PreorderedDistortionPenaltyState& rhs = dynamic_cast<const PreorderedDistortionPenaltyState&>(other);
        if (m_last_covered == rhs.GetLastCovered()) {
            return (m_last_covered_given_wa < rhs.GetLastCoveredGivenWA())? -1 : 1;
        } else {
            return (m_last_covered < rhs.GetLastCovered())? -1 : 1;
        }
    }
    
private:
    int m_last_covered;
    int m_last_covered_given_wa;
};


class PreorderedDistortionPenalty : public StatefulFeatureFunction
{
private:
    char m_unfold_heuristic; 
    std::vector< std::vector<std::size_t> > m_permutations; // table of permutations
    std::string m_mapping_path; // path to a possible list of s' to s mappings

    inline void SetExternalDistortionCost(std::vector<float> &scores, const float score) const
    {
        scores[0] = score;
    }
    
    inline void SetInternalDistortionCost(std::vector<float> &scores, const float score) const
    {
        scores[1] = score;
    }

    std::vector<std::size_t> MapPositions(const std::size_t sid, const WordsRange& range) const;

public:
  PreorderedDistortionPenalty(const std::string &line);

  bool IsUseable(const FactorMask &mask) const {
    return true;
  }
  
  void Load();
  
  inline const FFState* EmptyHypothesisState(const InputType &input) const
  {
      return new PreorderedDistortionPenaltyState(-1, -1);
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
    throw std::logic_error("PreorderedDistortionPenalty not valid in chart decoder");
  }

  void SetParameter(const std::string& key, const std::string& value);
  
  void InitializeForInput(InputType const& source); 


};

}


