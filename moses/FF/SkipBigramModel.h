#pragma once

#include <vector>
#include <map>
#include <string>
#include "moses/WordsRange.h"
#include "StatelessFeatureFunction.h"
#include "moses/Hypothesis.h"
#include "moses/ChartHypothesis.h"
#include "moses/InputPath.h"
#include "moses/AlignmentInfo.h"
#include "moses/FF/ReorderingHelper.h"

namespace Moses
{

/*
 * This feature is a skip-bigram model, that is, a feature that factorises as a sum of weights over skip-bigrams.
 * The typical use is to model the expected Kendall Tau of a hypothesis which does decomposes as expectations over skip-bigrams.
 *
 * It does not accept Lattice input, for that see LatticeSkipBigramModel.
 * It does not support chart-based decoding.
 * 
 * This is a reordering feature which only depends on the source segmentation.
 *
 * We compute
 *
 *      sum_{(i, j) \in \pi} w(i, j)
 *
 *  where 
 *      \pi is a permutation of the input,
 *      (i, j) is a skip bigram,
 *      and w(i, j) is the weight associated with (i, j)
 *          typically, the expectation that position i should be translated before position j.
 *
 * Arguments:
 *  model=<path>
 *      a table of skip-bigrams weights
 *      1 table per line, each cell in the table is a triplet i:j:w, cells can be separated by a space or tab
 *      where i and j are 0-based indices and w is the weight of skip-bigram (i, j)
 *  permutations=<path>
 *      this table is optional, if you do not provide it, then we assume the input is provided in original word-order
 *      if you do provide, we assume preordered input, then we interpret this as a table of permutations
 *      1 permutation per line
 *      each permutation is a space-separated sequence of 0-based positions mapping s' (preordered input) positions to s (original input) positions
 *  unfold=monotone|left|right
 *      to permute input paths into target word-order we use alignment information within phrase pairs
 *      when doing so we have a heuristic to deal with unaligned words, 
 *      we can attach them to the left or to the right;
 *      if you don't want to compute such permutations, you can use the option 'monotone' which will simply preserve the input path's word order
 *  missing=<float>
 *      in case we query an entry missing in the model we return this value (default: 0.0)
 *      By default, this feature assumes the model entries are expectations, that's why our missing value is 0.0
 *      If you have a model whose entries are log-probabilities, then you probably want to make sure your table is smoothed (no missing entries),
 *      or you should set this value to something like -99.0
 *
 * Features:
 *  1. Expected score external to phrases
 *      Whatever permutation of source words a phrase covers is ignored and only the contribution relative to words yet to be translated is taken into account.
 *  2. Expected score internal to phrases
 *      This is the part left out in the computation of (1), that is, it captures permutation operations covered by a contiguous phrase.
 *  3. Expected score internal to phrases given word alignment
 *      This is similar to 2, but it further permutes s' into target word-order by inspecting word alignments.
 *      A heuristic for unaligned words applies (see above).
 */
class SkipBigramModel : public StatelessFeatureFunction
{
private:
    std::vector< std::vector<std::size_t> > m_permutations; // table of permutations
    std::vector< std::map< std::size_t, std::map<std::size_t, double> > > m_model; // table of expectations
    std::string m_table_path; // path to table of expectations
    std::string m_mapping_path; // path to a possible list of s' to s mappings
    char m_unfold_heuristic; // selects a heuristic to unfold word alignments
    float m_missing; // what to use instead of log(0) (default: -99)

    
public:
  SkipBigramModel(const std::string &line);

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
                           ScoreComponentCollection* accumulator) const 
  {
      UTIL_THROW2("SkipBigramModel does not yet support chart-based decoding.")
  }


  void SetParameter(const std::string& key, const std::string& value);
  
  void Load();
  
  void InitializeForInput(InputType const& source);

private:
     
    /*
     * This method abstracts away the position of the feature "external score" in the vector of score components.
     *
     * "External score" is the score computed for a given phrase wrt the untranslated source words 
     */
    inline void SetScoreExternalToPhrase(std::vector<float> &scores, const float score) const
    {
        scores[0] = score;
    }

    /*
     * This method abstracts away the position of the feature "internal score" in the vector of score components.
     *
     * "Internal score" is the score computed for a given phrase as a function of the way source-target words 
     * align internally. This requires a heuristic to "unfold" or "linearize" the source in target word-order.
     */
    inline void SetScoreInternalToPhrase(std::vector<float> &scores, const float score) const
    {
        scores[1] = score;
    }
    
    inline void SetScoreInternalToPhraseGivenWA(std::vector<float> &scores, const float score) const
    {
        scores[2] = score;
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
    inline std::size_t MapInputPosition(const std::size_t sid, const std::size_t i) const
    {
        return ReorderingHelper::MapInputPosition(m_permutations, sid, i);
    }

};

}

