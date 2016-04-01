#include "moses/FF/SkipBigramModel.h"

#include <cstdlib>
#include <algorithm>
#include <vector>
#include <set>
#include <fstream>
#include <iostream>
#include "moses/WordsBitmap.h"
#include "moses/WordsRange.h"
#include "moses/Hypothesis.h"
#include "moses/ScoreComponentCollection.h"
#include "moses/TargetPhrase.h"
#include "moses/InputType.h"
#include "moses/Util.h"
#include "moses/TypeDef.h"

using namespace std;

namespace Moses
{

 SkipBigramModel::SkipBigramModel(const std::string &line)
  :StatelessFeatureFunction(3, line), m_unfold_heuristic('L'), m_missing(0.0)
{
    ReadParameters();
    // sanity checks
    UTIL_THROW_IF2(m_table_path.empty(),
          "SkipBigramModel requires a model file, that is, a list of table of skip-bigram weights: model=<path>");
}

/**
 * CONFIG FF
 */

void SkipBigramModel::SetParameter(const std::string& key, const std::string& value)
{
  if (key == "model" || key == "skipBigramExpLocation" || key == "table") {
        // read table of skip-bigram probabilities
        if (FileExists(value)) {
            m_table_path = value;
        } else {
            UTIL_THROW2("Expectation file not found: " + value);
        }
  } else if (key == "permutations" || key == "mapping") {
        if (FileExists(value)) {
            m_mapping_path = value;
        } else {
            UTIL_THROW2("Permutation file not found: " + value);
        }
  } else if (key == "targetWordOrderHeuristic" || key == "unfold") {
      if (value == "monotone")
          m_unfold_heuristic = 'M';
      else if (value == "unalignedAttachesToLeft" || value == "left") 
          m_unfold_heuristic = 'L';
      else if (value == "unalignedAttachesToRight" || value == "right") 
          m_unfold_heuristic = 'R';
      else
          UTIL_THROW2("Unknown heuristic: " + value);
  } else if (key == "missing") {
      m_missing = Scan<float>(value);
  } else {
        StatelessFeatureFunction::SetParameter(key, value);
  }
}

void SkipBigramModel::Load() 
{
    // read model
    ReorderingHelper::ReadSkipBigramTables(m_table_path, m_model);
    // read table of permutations
    if (!m_mapping_path.empty())
        ReorderingHelper::ReadPermutations(m_mapping_path, m_permutations);
}

/**
 * COMPUTE FF
 */
  
void SkipBigramModel::InitializeForInput(InputType const& source) 
{
    const std::size_t sid = source.GetTranslationId();  // the sequential id of the segment being translated
    
    // sanity checks
   
    UTIL_THROW_IF2(source.GetType() != SentenceInput,
            "SkipBigramModel only supports sentence input");

    UTIL_THROW_IF2(sid >= m_model.size(),
          "SkipBigramModel::InitializeForInput: it seems like you are missing entries in your model.");

    if (!m_permutations.empty()) {
        UTIL_THROW_IF2(sid >= m_permutations.size(),
              "SkipBigramModel::InitializeForInput: it seems like you are missing entries in the table of permutations.");

        UTIL_THROW_IF2(source.GetSize() != m_permutations[sid].size(),
              "SkipBigramModel::InitializeForInput: it seems like there is a mismatch in sentence/permutation length.");

    }

}

void SkipBigramModel::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{
    const std::size_t sid = input.GetTranslationId();
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // first we a permutation of input positions being covered
    std::vector<std::size_t> f_permutation(ReorderingHelper::MapInputPositions(m_permutations, sid, inputPath.GetWordsRange()));
    // score source phrase wrt how it permutes the original input
    SetScoreInternalToPhrase(scores, ReorderingHelper::ComputeExpectation(m_model, sid, f_permutation, m_missing));
    
    // score source phrase wrt its target word-order permutation given word alignment information
    // unaligned words behave according to the option of unfold heuristic
    std::vector<std::size_t> e_permutation(ReorderingHelper::GetPermutation(f_permutation, targetPhrase.GetAlignTerm(), m_unfold_heuristic));
    SetScoreInternalToPhraseGivenWA(scores, ReorderingHelper::ComputeExpectation(m_model, sid, e_permutation, m_missing));
    
    scoreBreakdown.PlusEquals(this, scores);
}

void SkipBigramModel::EvaluateWhenApplied(const Hypothesis& hypo,
    ScoreComponentCollection* accumulator) const
{
    const std::size_t sid = hypo.GetInput().GetTranslationId();
    std::vector<float> scores(GetNumScoreComponents(), 0.0);

    const WordsBitmap& bmap = hypo.GetWordsBitmap();
    const WordsRange& curr = hypo.GetCurrSourceWordsRange();
    // isolate untranslated words as those will necessarily appear to the right of the current phrase
    std::vector<std::size_t> right; 
    right.reserve(bmap.GetSize() - bmap.GetNumWordsCovered());

    // collect information from coverage vector
    for (std::size_t i = 0; i < curr.GetStartPos(); ++i) { 
        if (!bmap.GetValue(i)) {
            right.push_back(MapInputPosition(sid, i));
        }
    }
    for (std::size_t i = curr.GetEndPos() + 1; i < bmap.GetSize(); ++i) {
        if (!bmap.GetValue(i)) {
            right.push_back(MapInputPosition(sid, i));
        }
    }

    // positions in the current phrase
    std::vector<std::size_t> left(ReorderingHelper::MapInputPositions(m_permutations, sid, curr));

    SetScoreExternalToPhrase(scores, ReorderingHelper::ComputeExpectation(m_model, sid, left, right, m_missing));
    accumulator->PlusEquals(this, scores);
}

}

