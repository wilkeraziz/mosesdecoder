#include "moses/FF/PermutationExpectedKendallTau.h"

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

using namespace std;

namespace Moses
{

PermutationExpectedKendallTau::PermutationExpectedKendallTau(const std::string &line)
  :StatefulFeatureFunction(3, line), 
    m_unfold_heuristic('M'), 
    m_internal_scoring(true),
    m_wa_scoring(true),
    m_sstate_fname("index")
{
    ReadParameters();
    
    // sanity checks
    UTIL_THROW_IF2(m_table_path.empty(),
          "PermutationExpectedKendallTau requires a table of expectations of skip-bigrams (table=<path>).");
    UTIL_THROW_IF2(m_length_table_path.empty(),
          "PermutationExpectedKendallTau requires a table containing the length of the original sentences (length-table=<path>).");
    
    // update the number of components
    /*
    std::size_t n_components = 0;
    if (m_wa_scoring) {
        n_components += (m_internal_scoring)? 3 : 1;
    } else {
        n_components += (m_internal_scoring)? 2 : 1;
    } 
    m_numScoreComponents = n_components;  // not sure how we get to have the number of features depend on the parameters of the FF
    */
}

/**
 * CONFIG FF
 */

void PermutationExpectedKendallTau::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "skipBigramExpLocation" || key == "table") {
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
    } else {
        FeatureFunction::SetParameter(key, value);
    }
}

/**
 * LOAD FILES
 */


void PermutationExpectedKendallTau::Load() 
{
    // read expectations if provided
    if (!m_length_table_path.empty())
        ReadLengthInfo(m_length_table_path);
    if (!m_table_path.empty())
        ReadExpectations(m_table_path);
}

void PermutationExpectedKendallTau::ReadExpectations(const std::string& path)
{
    std::ifstream fin(path.c_str());
    std::string line;
    while (std::getline(fin, line)) {
        const std::vector<std::string> tokens = Tokenize(Trim(line), " \t:");
        m_taus.resize(m_taus.size() + 1);
        std::map< std::size_t, std::map<std::size_t, double> > &taus = m_taus.back();
        std::size_t length = 0;
        UTIL_THROW_IF2(tokens.size() % 3 != 0,
          "Line " + SPrint(m_taus.size()) + ". Expected triplets (i:j:u), got: " + line);
        if (tokens.size() > 0) {
            for (std::size_t i = 0; i < tokens.size() - 2; i += 3) {
                const std::size_t a = Scan<std::size_t>(tokens[i]);
                const std::size_t b = Scan<std::size_t>(tokens[i + 1]);
                const double expectation = Scan<double>(tokens[i + 2]);
                taus[a][b] = expectation; 
                if (a + 1 > length)
                    length = a + 1;
                if (b + 1 > length)
                    length = b + 1;
            }
        }
    }
}

void PermutationExpectedKendallTau::ReadLengthInfo(const std::string& path)
{
    m_lengths.clear();
    std::ifstream fin(path.c_str());
    std::string line;
    while (std::getline(fin, line)) {
        const std::size_t length = Scan<std::size_t>(line);
        m_lengths.push_back(length);
    }
}

/**
 * HELPER
 */

double PermutationExpectedKendallTau::GetExpectation(const std::size_t sid, const std::size_t left, const std::size_t right) const
{
    std::map< std::size_t, std::map<std::size_t, double> >::const_iterator it = m_taus[sid].find(left);
    if (it == m_taus[sid].end()) 
        return 0.0;
    std::map<std::size_t, double>::const_iterator it2 = it->second.find(right);
    if (it2 == it->second.end())
        return 0.0;
    return it2->second;
}


/**
 * COMPUTE FF
 */
  
void PermutationExpectedKendallTau::InitializeForInput(InputType const& source) 
{
    const std::size_t sid = source.GetTranslationId();  // the sequential id of the segment being translated
    // sanity checks
    UTIL_THROW_IF2(sid >= m_taus.size(),
          "PermutationExpectedKendallTau::InitializeForInput: it seems like you are missing entries in the table of skip-bigram expectations.");
}


float PermutationExpectedKendallTau::ComputeExpectation(const std::size_t sid, std::vector<std::size_t> &positions) const
{
    float expectation = 0.0;
    for (std::size_t i = 0; i < positions.size() - 1; ++i) {
        const std::size_t left = positions[i];
        for (std::size_t j = i + 1; j < positions.size(); ++j) {
            const std::size_t right = positions[j];
            expectation += GetExpectation(sid, left, right);
        }
    }
    return expectation;
}

float PermutationExpectedKendallTau::ComputeExpectation(const std::size_t sid, const std::vector<std::size_t>& positions, const WordsBitmap& coverage) const
{
    float expectation = 0.0;
    for (std::size_t right = 0; right < coverage.GetSize(); ++right) {
        if (coverage.GetValue(right))  // translated words have already been scored
            continue;
        for (std::size_t i = 0; i < positions.size(); ++i) { 
            const std::size_t left = positions[i];
            expectation += GetExpectation(sid, left, right);
        }
    }
    return expectation;
}


void PermutationExpectedKendallTau::EvaluateWithSourceContext(const InputType &input
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
    std::vector<std::size_t> positions(GetInputPositions(inputPath, m_sstate_fname)); 
    SetKTauInternalToPhrase(scores, ComputeExpectation(sid, positions));

    if (m_wa_scoring) { // maybe we want to rely on word-alignment
        // score phrases wrt how they permute the input given word alignment information (this rearranges the lattice input in target word order)
        // unaligned words behave according to the option of unfold heuristic
        std::vector<std::size_t> permutation(GetPermutation(positions, targetPhrase.GetAlignTerm(), m_unfold_heuristic));
        SetKTauInternalToPhraseGivenWA(scores, ComputeExpectation(sid, permutation));
    }

    // update feature vector 
    scoreBreakdown.PlusEquals(this, scores);

}


FFState* PermutationExpectedKendallTau::EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
      ScoreComponentCollection* accumulator) const
{
    const std::size_t sid = hypo.GetInput().GetTranslationId();
    // here we need to score the hypothesis wrt yet untranslated words
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // first we need to get information from the input path that the current phrase covers
    const TranslationOption& topt = hypo.GetTranslationOption();
    const InputPath& path = topt.GetInputPath(); 
    // from the arcs of the lattice we get an input permutation
    std::vector<std::size_t> positions = GetInputPositions(path, m_sstate_fname);
    
    // now we can update the current coverage vector
    const PermutationExpectedKendallTauState* prev = dynamic_cast<const PermutationExpectedKendallTauState*>(prev_state);  // previous coverage
    WordsBitmap coverage(prev->GetCoverage());
    for (std::size_t i = 0; i < positions.size(); ++i)
        coverage.SetValue(positions[i], true);  

    // we update the score external to the phrase (notice that this feature never depends on word alignment)
    // because the internal order of the phrase does not matter
    SetKTauExternalToPhrase(scores, ComputeExpectation(sid, positions, coverage));

    // and return the new coverage vector  
    accumulator->PlusEquals(this, scores);
    return new PermutationExpectedKendallTauState(coverage);

}



}


