#include "moses/FF/LatticeSkipBigramModel.h"

#include <cstdlib>
#include <string>
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

LatticeSkipBigramModel::LatticeSkipBigramModel(const std::string &line)
  :StatefulFeatureFunction(3, line), 
    m_unfold_heuristic('M'), 
    m_internal_scoring(true),
    m_wa_scoring(true),
    m_sstate_fname("index"),
    m_missing(0.0)
{
    ReadParameters();
    
    // sanity checks
    UTIL_THROW_IF2(m_table_path.empty(),
          "LatticeSkipBigramModel requires a table of expectations of skip-bigrams (table=<path>).");
    UTIL_THROW_IF2(m_length_table_path.empty(),
          "LatticeSkipBigramModel requires a table containing the length of the original sentences (length-table=<path>).");
    
}

/**
 * CONFIG FF
 */

void LatticeSkipBigramModel::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "model" || key == "skipBigramExpLocation" || key == "table") {
        // read table of skip-bigram expectations
        if (FileExists(value)) {
            m_table_path = value;
        } else {
            UTIL_THROW2("Table not found: " + value);
        }
    } else if (key == "originalLengthTable" || key == "length-table") {
        if (FileExists(value)) {
            m_length_table_path = value;
        } else {
            UTIL_THROW2("Length table not found: " + value);
        }
    } else if (key == "originalPosLabel" || key == "sstate-fname") {
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
    } else if (key == "missing") {
        m_missing = Scan<float>(value);
    } else {
        FeatureFunction::SetParameter(key, value);
    }
}

/**
 * LOAD FILES
 */


void LatticeSkipBigramModel::Load() 
{
    // read expectations if provided
    if (!m_length_table_path.empty())
        ReorderingHelper::ReadLengthInfo(m_length_table_path, m_lengths);
    if (!m_table_path.empty())
        ReorderingHelper::ReadSkipBigramTables(m_table_path, m_taus);
}

/**
 * COMPUTE FF
 */
  
void LatticeSkipBigramModel::InitializeForInput(InputType const& source) 
{
    // sanity checks
    UTIL_THROW_IF2(source.GetType() != ConfusionNetworkInput,  // Moses can be so ugly some times :( how can a WordLattice be a type of ConfusionNetwork and not the other way around?
        "LatticeSkipBigramModel only supports lattice input (for sentence input see SkipBigramModel)");
    UTIL_THROW_IF2(source.GetTranslationId() >= (long)m_taus.size(),
          "LatticeSkipBigramModel::InitializeForInput: it seems like you are missing entries in the table of skip-bigram expectations.");
}


void LatticeSkipBigramModel::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{
    if (!m_internal_scoring) // maybe we don't care about internal scoring
        return;

    const std::size_t sid = input.GetTranslationId();
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // score phrases wrt how they permute the input (this uses the lattice input and ignores word alignment)
    std::vector<std::size_t> positions(ReorderingHelper::GetInputPositionsFromArcs(inputPath, m_sstate_fname)); 
    SetKTauInternalToPhrase(scores, ReorderingHelper::ComputeExpectation(m_taus, sid, positions, m_missing));

    if (m_wa_scoring) { // maybe we want to rely on word-alignment
        // score phrases wrt how they permute the input given word alignment information (this rearranges the lattice input in target word order)
        // unaligned words behave according to the option of unfold heuristic
        std::vector<std::size_t> permutation(ReorderingHelper::GetPermutation(positions, targetPhrase.GetAlignTerm(), m_unfold_heuristic));
        SetKTauInternalToPhraseGivenWA(scores, ReorderingHelper::ComputeExpectation(m_taus, sid, permutation, m_missing));
    }

    // update feature vector 
    scoreBreakdown.PlusEquals(this, scores);

}


FFState* LatticeSkipBigramModel::EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
      ScoreComponentCollection* accumulator) const
{
    const std::size_t sid = hypo.GetInput().GetTranslationId();
    // here we need to score the hypothesis wrt yet untranslated words
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // first we need to get information from the input path that the current phrase covers
    const TranslationOption& topt = hypo.GetTranslationOption();
    const InputPath& path = topt.GetInputPath(); 
    // from the arcs of the lattice we get an input permutation
    std::vector<std::size_t> positions = ReorderingHelper::GetInputPositionsFromArcs(path, m_sstate_fname);
    
    // now we can update the current coverage vector
    const LatticeSkipBigramModelState* prev = dynamic_cast<const LatticeSkipBigramModelState*>(prev_state);  // previous coverage
    WordsBitmap coverage(prev->GetCoverage());
    for (std::size_t i = 0; i < positions.size(); ++i)
        coverage.SetValue(positions[i], true);  

    // we update the score external to the phrase (notice that this feature never depends on word alignment)
    // because the internal order of the phrase does not matter
    SetKTauExternalToPhrase(scores, ReorderingHelper::ComputeExpectation(m_taus, sid, positions, coverage, m_missing));

    // and return the new coverage vector  
    accumulator->PlusEquals(this, scores);
    return new LatticeSkipBigramModelState(coverage);

}



}


