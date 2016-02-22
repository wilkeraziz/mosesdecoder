#include <cstdlib>
#include <algorithm>
#include <vector>
#include <deque>
#include <set>
#include <fstream>
#include <iostream>
#include "SourcePermutationLM.h"
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

using namespace std;


namespace Moses
{

 SourcePermutationLM::SourcePermutationLM(const std::string &line)
  :StatefulFeatureFunction(1, line)
{
    ReadParameters();
    UTIL_THROW_IF2(m_model_path.empty(),
          "SourcePermutationLM requires a kenlm-compatible language model (model=<path>).");
}

/**
 * CONFIG FF
 */

void SourcePermutationLM::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "model") {
        if (FileExists(value)) {
            m_model_path = value;
        } else {
            UTIL_THROW2("Model not found: " + value);
        }
    } else {
        FeatureFunction::SetParameter(key, value);
    }
}

/**
 * LOAD FILES
 */

void SourcePermutationLM::Load() 
{
    m_model.reset(new kenlm::ngram::Model(m_model_path.c_str()));
}

/**
 * COMPUTE FF
 */

float SourcePermutationLM::KenLMScore(const Phrase& phrase, 
        const std::size_t begin, 
        const std::size_t end, 
        kenlm::ngram::State in_state, 
        kenlm::ngram::State *out_state) const
{
    kenlm::ngram::State aux_state;
    kenlm::ngram::State *state0 = &in_state;
    kenlm::ngram::State *state1 = &aux_state;
    float score = 0.0;
    for (std::size_t i = begin; i < end; ++i) {   
        score += m_model->Score(*state0, 
                m_model->GetVocabulary().Index(phrase.GetWord(i).GetString(0)), 
                *state1);
        std::swap(state0, state1);
    }
    *out_state = *state0;
    return score;
}


float SourcePermutationLM::KenLMScore(const Phrase& phrase, 
        float &outside) const
{
    kenlm::ngram::State in_state = m_model->NullContextState();
    kenlm::ngram::State out_state;
    kenlm::ngram::State *state0 = &in_state;
    kenlm::ngram::State *state1 = &out_state;
    const std::size_t ctxt_size = m_model->Order() - 1;
    outside = 0.0;
    float score = 0.0;
    for (std::size_t i = 0; i < phrase.GetSize(); ++i) {   
        float partial = m_model->Score(*state0, 
                m_model->GetVocabulary().Index(phrase.GetWord(i).GetString(0)), 
                *state1);
        std::swap(state0, state1);
        if (i >= ctxt_size) {
            score += partial;
        } else {
            outside += partial;
        }
    }
    return score;
}
  
void SourcePermutationLM::EvaluateInIsolation(const Phrase &source
                       , const TargetPhrase &targetPhrase
                       , ScoreComponentCollection &scoreBreakdown
                       , ScoreComponentCollection &estimatedFutureScore) const
{
    float outside = 0.0;
    float score = KenLMScore(source, outside);
    // transform scores and add to vector
    scoreBreakdown.Assign(this, TransformLMScore(score));
    estimatedFutureScore.Assign(this, TransformLMScore(outside));
}


FFState* SourcePermutationLM::EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
      ScoreComponentCollection* accumulator) const
{
    //accumulator->PlusEquals(this, 0.0);
    const TranslationOption& topt = hypo.GetTranslationOption();
    const InputPath& path = topt.GetInputPath();
    const Phrase& f_phrase = path.GetPhrase();
    const SourcePermutationLMState* prev = dynamic_cast<const SourcePermutationLMState*>(prev_state);  
    if (!f_phrase.GetSize()) {  // empty phrase
        return new SourcePermutationLMState(prev->GetKenLMState());  // state does not change
    }

    // score the leftmost incomplete ngram in context
    kenlm::ngram::State out_state;
    const std::size_t max_ctxt_size = m_model->Order() - 1;
    const std::size_t end = std::min(f_phrase.GetSize(), max_ctxt_size);
    float score = KenLMScore(f_phrase, 0, end, prev->GetKenLMState(), &out_state);

    // update output state if the phrase is long enough
    if (end < f_phrase.GetSize()) {  // phrase is too long, we need to update the output state
        if (f_phrase.GetSize() - end < m_model->Order()) { // in this case we can continue from the current output state
            // we do not incorporate the score, we just update state
            KenLMScore(f_phrase, end, f_phrase.GetSize(), out_state, &out_state);
        } else {  // in this case it is faster to simply retrieve the state using the last n - 1 words
            // we do not incorporate the score, we just retrieve the output state
            KenLMScore(f_phrase, f_phrase.GetSize() - max_ctxt_size, f_phrase.GetSize(), m_model->NullContextState(), &out_state);
        }
    }

    if (hypo.IsSourceCompleted()) { // incorporate end-of-sentence
        kenlm::ngram::State aux_state = out_state;
        score += m_model->Score(aux_state, m_model->GetVocabulary().EndSentence(), out_state);
    }

    accumulator->PlusEquals(this, TransformLMScore(score));

    // TODO: OOV feature?
    
    return new SourcePermutationLMState(out_state);
}

  



}

