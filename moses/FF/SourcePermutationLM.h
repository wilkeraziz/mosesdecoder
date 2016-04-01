#pragma once

#include <vector>
#include <map>
#include <string>
#include "moses/WordsRange.h"
#include "moses/FF/StatefulFeatureFunction.h"
#include "moses/Hypothesis.h"
#include "moses/ChartHypothesis.h"
#include "moses/FF/FFState.h"
#include "moses/Util.h"
#include "lm/model.hh"

namespace kenlm = ::lm;

namespace Moses
{

/*
 * This feature scores source permutations by computing a language model score over some input factors.
 * Typical uses:
 *  1. one has a language model over gold permutations of the input
 *  2. one has a language model over gold permutations of POS tags
 *
 * This feature can deal with both sentence and lattice input. It does not need access to permutation mappings, instead, 
 * it only consults the actual factors being consumed in a given order.
 * This is obviously a stateful feature as any other language model.
 * The difference is that there shouldn't be as much ambiguity here as in standard target LM rescoring.
 * Of course, this depends on how diverse the space of permutation is.
 *
 * Features:
 *  1. LM score
 */
class SourcePermutationLMState : public FFState
{
public:
    SourcePermutationLMState(kenlm::ngram::State kenlm_state) : m_kenlm_state(kenlm_state) 
    {}

    inline int Compare(const FFState& other) const
    {
        const SourcePermutationLMState& rhs = dynamic_cast<const SourcePermutationLMState&>(other);
        const kenlm::ngram::State& mine = m_kenlm_state;
        const kenlm::ngram::State& theirs = rhs.GetKenLMState();
        if (mine.length < theirs.length) return -1;
        if (mine.length > theirs.length) return 1;
        return std::memcmp(mine.words, theirs.words, sizeof(kenlm::WordIndex) * mine.length);
    }

    inline const kenlm::ngram::State& GetKenLMState() const
    {
        return m_kenlm_state;
    }
    
private:
    const kenlm::ngram::State m_kenlm_state;

};


/*
 */
class SourcePermutationLM : public StatefulFeatureFunction
{
private:
    std::string m_model_path;  // where the LM is stored
    boost::shared_ptr<kenlm::ngram::Model> m_model;
    int m_factor;


public:
  SourcePermutationLM(const std::string &line);

  bool IsUseable(const FactorMask &mask) const {
    return true;
  }
  
  void Load();
  
  inline const FFState* EmptyHypothesisState(const InputType &input) const
  {
      SourcePermutationLMState *state = new SourcePermutationLMState(m_model->BeginSentenceState());
      return state;
  }
 
  // TODO: Moses does not call EvaluateInIsolation (with an inputPath) 
  // it calls EvaluateInIsolation with an Phrase 
  // I need to assess the inputPath
  // What Moses does call with an inputPath is EvaluateWithSourceContext
  // but that method does not get called for OOVs
  // so I had to invent this alternative here 
  void EvaluateInIsolation(const InputPath &inputPath
                           , const TargetPhrase &targetPhrase
                           , ScoreComponentCollection &scoreBreakdown
                           , ScoreComponentCollection &estimatedFutureScore) const;
  
  void EvaluateTranslationOptionListWithSourceContext(const InputType &input
      , const TranslationOptionList &translationOptionList) const
  {
  }
  
  FFState* EvaluateWhenApplied(const ChartHypothesis& hypo,
          int featureID,
          ScoreComponentCollection* accumulator) const 
  {
    UTIL_THROW2("SourcePermutationLM does not support chart-based decoding.");
  }


  void EvaluateWithSourceContext(const InputType &input
                                 , const InputPath &inputPath
                                 , const TargetPhrase &targetPhrase
                                 , const StackVec *stackVec
                                 , ScoreComponentCollection &scoreBreakdown
                                 , ScoreComponentCollection *estimatedFutureScore) const;
  
  // I cannot use this one, because source is a Phrase from the phrase table, not from the input!
  void EvaluateInIsolation(const Phrase &source
                           , const TargetPhrase &targetPhrase
                           , ScoreComponentCollection &scoreBreakdown
                           , ScoreComponentCollection &estimatedFutureScore) const
  {
  }


  FFState* EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
          ScoreComponentCollection* accumulator) const;

  void SetParameter(const std::string& key, const std::string& value);
  
  void InitializeForInput(InputType const& source)
  {
  }

private:

    /**
     * Computes the KenLM score.
     *
     * Use STL-style [begin, end) intervals.
     */
    float KenLMScore(const Phrase& phrase, 
            const std::size_t begin, 
            const std::size_t end, 
            kenlm::ngram::State in_state, 
            kenlm::ngram::State *out_state) const;

    /**
     * Returns the KenLM score associated with complete n-grams assuming a null context state.
     * Additionally, computes an estimate of the outside score (that of the incomplete n-grams).
     */
    float KenLMScore(const Phrase& phrase, float &outside) const;
        
};

}

