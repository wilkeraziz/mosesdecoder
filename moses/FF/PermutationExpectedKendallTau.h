#pragma once

#include <vector>
#include <map>
#include <string>
#include "moses/WordsRange.h"
#include "moses/FF/StatefulFeatureFunction.h"
#include "moses/Hypothesis.h"
#include "moses/ChartHypothesis.h"
#include "moses/FF/FFState.h"
#include "moses/FF/LatticePermutationFeatures.h"

namespace Moses
{
    
class PermutationExpectedKendallTauState : public FFState
{
public:
    PermutationExpectedKendallTauState(const WordsBitmap& coverage):
        m_coverage(coverage)
    {}

    inline const WordsBitmap& GetCoverage() const 
    {
        return m_coverage;
    }

    inline int Compare(const FFState& other) const
    {
        const PermutationExpectedKendallTauState& rhs = dynamic_cast<const PermutationExpectedKendallTauState&>(other);
        return m_coverage.Compare(rhs.GetCoverage());
    }
    
private:
    const WordsBitmap m_coverage;
};


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
 *
 *  Components:
 *
 *      1. External KTau
 *      2. Internal KTau
 *      3. Internal KTau Given Word Alignment
 */
class PermutationExpectedKendallTau : public StatefulFeatureFunction
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

    std::string m_table_path; // path to table of expectations
    std::string m_length_table_path; // path to table of lengths
    std::vector< std::map< std::size_t, std::map<std::size_t, double> > > m_taus; // table of expectations
    std::vector<std::size_t> m_lengths; // the length of each original source
    std::string m_sstate_fname;  // name of arc feature representing the state in s (original source)

public:
  PermutationExpectedKendallTau(const std::string &line);

  bool IsUseable(const FactorMask &mask) const {
    return true;
  }
  
  void Load();
  
  inline const FFState* EmptyHypothesisState(const InputType &input) const
  {
      return new PermutationExpectedKendallTauState(WordsBitmap(GetInputLength(input.GetTranslationId())));
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
  //void EvaluateWhenApplied(const Hypothesis& hypo,
  //                         ScoreComponentCollection* accumulator) const;
  FFState* EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
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
  //void EvaluateWhenApplied(const ChartHypothesis &hypo,
  //                         ScoreComponentCollection* accumulator) const;
  FFState* EvaluateWhenApplied(const ChartHypothesis& hypo,
          int featureID,
          ScoreComponentCollection* accumulator) const {
    throw std::logic_error("PermutationExpectedKendallTau not valid in chart decoder");
  }

  void SetParameter(const std::string& key, const std::string& value);
  
  void InitializeForInput(InputType const& source); 
  // TODO - CHECK: Learn more about the contract in this case. I think we need to make sure ourselves that this is thread-safe... which makes me wonder why this method is not const. I agree it does not make sense to have an "initialization" method be const, but then we should have a clearer contract.

private:

    inline std::size_t GetInputLength(const std::size_t sid) const 
    {
        return m_lengths[sid];
    }  
    
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
     * Load length info about the original input sentences.
     * This is necessary in order to instantiate a coverage vector of appropriate length.
     * Format: one integer per line.
     */
    void ReadLengthInfo(const std::string& path);

    /*
     * Return the contribution of the skip bigram <left ... right> to the 
     * expected Tau. The variables 'left' and 'right' are interpreted as 0-based positions in s.
     * 
     * Tip: the common way to use this method is 
     *  GetExpectation(MapInputPosition(i), MapInputPosition(j)) 
     * where MapInputPosition abstracts away from the type of input (original vs predicted order).
     */
    double GetExpectation(const std::size_t sid, const std::size_t left, const std::size_t right) const;

    float ComputeExpectation(const std::size_t sid, std::vector<std::size_t> &positions) const;
    float ComputeExpectation(const std::size_t sid, const std::vector<std::size_t>& positions, const WordsBitmap& coverage) const;
    
    /*
     * This method abstracts away the position of the feature "External KTau" in the vector of score components.
     *
     * "External KTau" is the Kendal Tau feature computed for a given phrase wrt the untranslated source words 
     */
    inline void SetKTauExternalToPhrase(std::vector<float> &scores, const float score) const
    {
        scores[0] = score;
    }

    /*
     * This method abstracts away the position of the feature "Internal KTau" in the vector of score components.
     * In this version we IGNORE word alignment information, thus the phrase is assumed to be covered in the order 
     * observed in the lattice.
     */
    inline void SetKTauInternalToPhrase(std::vector<float> &scores, const float score) const
    {
        scores[1] = score;
    }
    
    /*
     * This method abstracts away the position of the feature "Internal KTau Given Word Alignment" in the vector of score components.
     * In this version we USE word alignment information to rearrange the lattice input in target word order.
     * Because this is a function of the way source-target words align internally, we need a heuristic to deal with unaligned words.
     */
    inline void SetKTauInternalToPhraseGivenWA(std::vector<float> &scores, const float score) const
    {
        scores[2] = score;
    }
    
};

}


