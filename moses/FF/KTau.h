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
class KTau : public StatelessFeatureFunction
{
private:
    std::vector< std::vector<std::size_t> > m_permutations; // table of permutations
    std::vector< std::map< std::size_t, std::map<std::size_t, double> > > m_taus; // table of expectations
    std::string m_table_path; // path to table of expectations
    std::string m_mapping_path; // path to a possible list of s' to s mappings
    char m_unfold_heuristic; // selects a heuristic to unfold word alignments
    
public:
  KTau(const std::string &line);

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
  
  void Load();
  
  void InitializeForInput(InputType const& source);

private:
     
    /*
     * This method abstracts away the position of the feature "external KTau" in the vector of score components.
     *
     * "External KTau" is the Kendal Tau feature computed for a given phrase wrt the untranslated source words 
     */
    inline void SetKTauExternalToPhrase(std::vector<float> &scores, const float score) const
    {
        scores[0] = score;
    }

    /*
     * This method abstracts away the position of the feature "internal KTau" in the vector of score components.
     *
     * "Internal KTau" is the Kendal Tau feature computed for a given phrase as a function of the way source-target words 
     * align internally. This requires a heuristic to "unfold" or "linearize" the source in target word-order.
     */
    inline void SetKTauInternalToPhrase(std::vector<float> &scores, const float score) const
    {
        scores[1] = score;
    }

    /*
     * This method abstracts away from different settings of the decoder wrt 
     * the type of input being translate (namely, original or predicted word order).
     *
     * If we translating s (original word order), then this mapping is simply the identity function.
     * Sometimes however, we translate a permutation (s') of the original input (s)
     * (e.g. the best predicted order according to some reordering model)
     * in such cases we need to find the word in s which corresponds to a given ith word in s'.
     */
    std::size_t MapInputPosition(const std::size_t sid, const std::size_t i) const;

    /*
     * Return the contribution of the skip bigram <left ... right> to the 
     * expected Tau. The variables 'left' and 'right' are interpreted as 0-based positions in s.
     * 
     * Tip: the common way to use this method is 
     *  GetExpectation(MapInputPosition(i), MapInputPosition(j)) 
     * where MapInputPosition abstracts away from the type of input (original vs predicted order).
     */
    double GetExpectation(const std::size_t sid, const std::size_t left, const std::size_t right) const;

    /*
     * Load a file of expectation over skip bigrams.
     * Format: one distribution per line, each associated with an input segment.
     *  A line contains space (or tab) separated triplets. 
     *  A triplet (i, j, e) specifies the expectation of skip brigram <i ... j>
     *  where i and j are 0-based positions in s. 
     *  Within a triplet each field is separated by a pace (or a tab, or a colon). 
     */
    void ReadExpectations(const std::string& path);

    /*
     * Load a file of permutations: mappings from s' to s.
     * This file is necessary when the input segment being translated is itself a permutation (s') 
     * of some original sequence (s).
     * Format: one permutation per line, each associated with an input segment.
     *  A line is a list of 0-based integers, the ith number represents the position of the ith s' token in s. 
     */
    void ReadPermutations(const std::string& path);

    /*
     * Return the source permutation within a phrase: a function of source-target word alignments.
     * It also shifts positions so that they represent the span of the input being covered.
     */
    std::vector<std::size_t> GetPermutation(const WordsRange& currWordRange, const TargetPhrase& targetPhrase) const;

};

}

