#pragma once

#include <vector>
#include <map>
#include <string>
#include "moses/WordsRange.h"
#include "StatelessFeatureFunction.h"
#include "moses/Hypothesis.h"
#include "moses/ChartHypothesis.h"

namespace Moses
{

/*
 * This features assess the expected Kendall Tau of hypothesis.
 * It applies both to phrase-based (supported) and hiero-style (working on it) decoders.
 * 
 * This is a reordering feature which only depends on the source segmentation.
 *
 * We compute
 *
 *      sum_{(i, j) \in s'} exp(i, j)
 *
 *  where s' is the permutation implicitly defined by word alignment and segmentation,
 *  (left, right) is a skip bigram,
 *  and exp(left, right) is the expectation that position i should be translated before position j.
 */
class LatticeKTauSimple : public StatelessFeatureFunction
{
private:
    std::size_t m_seg; // segment being translated (necessary in referring to table of permutations and expectations)
    std::string m_fname;
    bool m_isolation;
    
public:
  LatticeKTauSimple(const std::string &line);

  bool IsUseable(const FactorMask &mask) const {
    return true;
  }
  
  // we have nothing to do here
  void EvaluateInIsolation(const Phrase &source
                           , const TargetPhrase &targetPhrase
                           , ScoreComponentCollection &scoreBreakdown
                           , ScoreComponentCollection &estimatedFutureScore) const
  {
  }
  
  void EvaluateInIsolation(const InputPath &inputPath
                           , const TargetPhrase &targetPhrase
                           , ScoreComponentCollection &scoreBreakdown
                           , ScoreComponentCollection &estimatedFutureScore) const;

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

  /*
   * Here the source segmentation is known.
   * Because PB decoding generates the target from left to right, we know
   * exactly everything we need to know in order to capture the full contribution 
   * of the given phrase in context.
   *
   * This is a rather global view of the decoding process packed in a local feature.
   *
   * Suppose a given phrase spanning from i..j (present),
   *  suppose some words have already been translated (past)
   *  others are yet to be translated (future).
   * There are skip bigrams of the kind (a, b) where a \in past and b \in present
   *  skip bigrams of the kind (b, c) where b \in present and c \in future.
   * Here we only score the latter as the former has already been incorporated
   *  when b (in present) was a member of some set of untranslated words at the time `a` was translated.
  */
  void EvaluateWhenApplied(const Hypothesis& hypo,
                           ScoreComponentCollection* accumulator) const;

  /*
   * Here we compute our feature in the context of a chart-based decoder.
   * Again all we require is the segmentation which is a byproduct of parsing.
   *
   * In PB-decoding, the target is generated left-to-right, thus the placement of a biphrase
   * determines a prefix permutation. Here, the target is not generated left-to-right, 
   * thus we can only reason about the permutation of the words under the LHS cell
   * (that is, there is no notion of prefix, instead we work with spans).
   * Scores are upadtes as ranges are concatenated (while observing the permutations of terminals and
   * nonterminal symbols).
   */
  void EvaluateWhenApplied(const ChartHypothesis &hypo,
                           ScoreComponentCollection* accumulator) const;


  void SetParameter(const std::string& key, const std::string& value);
  
  void InitializeForInput(InputType const& source);

private:
     
    inline void SetKTau(std::vector<float> &scores, const float score) const
    {
        scores[0] = score;
    }

};

}

