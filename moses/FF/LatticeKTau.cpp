#include "moses/FF/LatticeKTau.h"

#include <cstdlib>
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
#include "moses/FF/LatticePermutationFeatures.h"

using namespace std;

namespace  // HELPER stuff useful in this file only
{

// Computes the usual distortion cost function
int ComputeDistortionCost(const std::size_t left, const std::size_t right)
{
    int jump = right - left - 1;
    return (jump < 0)? -jump : jump;
}

// Computes distortion cost given a sequence of positions
int ComputeDistortionCost(const std::vector<std::size_t> &positions)
{
    if (positions.empty())
        return 0;
    int d = 0;
    int last_covered = positions.front();
    for (std::size_t i = 1; i < positions.size(); ++i) {
        d += ComputeDistortionCost(last_covered, positions[i]);
        last_covered = positions[i];
    }
    return d;
}

} 

namespace Moses
{

LatticeKTau::LatticeKTau(const std::string &line)
  :StatefulFeatureFunction(7, line), m_unfold_heuristic('M'), m_sstate_fname("sstate")
{
    ReadParameters();
    // sanity checks
    UTIL_THROW_IF2(m_table_path.empty(),
          "LatticeKTau requires a table of expectations of skip-bigrams (table=<path>).");
}

/**
 * CONFIG FF
 */

void LatticeKTau::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "skipBigramExpLocation" || key == "table") {
        // read table of skip-bigram expectations
        if (FileExists(value)) {
            m_table_path = value;
        } else {
            UTIL_THROW2("Table not found: " + value);
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
    } else {
        FeatureFunction::SetParameter(key, value);
    }
}

/**
 * LOAD FILES
 */


void LatticeKTau::Load() 
{
    // read expectations if provided
    if (!m_table_path.empty())
        ReadExpectations(m_table_path);
}


void LatticeKTau::ReadExpectations(const std::string& path) 
{
    std::ifstream fin(path.c_str());
    std::string line;
    while (std::getline(fin, line)) {
        const std::vector<std::string> tokens = Tokenize(line, " \t:");
        std::map< std::size_t, std::map<std::size_t, double> > taus;
        std::size_t length = 0;
        for (std::size_t i = 0; i < tokens.size() - 2; i += 3) {
            const std::size_t a = std::atoi(tokens[i].c_str());
            const std::size_t b = std::atoi(tokens[i + 1].c_str());
            const double expectation = std::atof(tokens[i + 2].c_str());
            taus[a][b] = expectation; 
            if (a + 1 > length)
                length = a + 1;
            if (b + 1 > length)
                length = b + 1;
        }
        m_taus.push_back(taus);
        m_lengths.push_back(length);
    }
}

/**
 * HELPER
 */

double LatticeKTau::GetExpectation(const std::size_t sid, const std::size_t left, const std::size_t right) const
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
  
void LatticeKTau::InitializeForInput(InputType const& source) 
{
    const std::size_t sid = source.GetTranslationId();  // the sequential id of the segment being translated
    // sanity checks
    UTIL_THROW_IF2(sid >= m_taus.size(),
          "LatticeKTau::GetExpectation: it seems like you are missing entries in the table of skip-bigram expectations.");
}


float LatticeKTau::ComputeExpectation(const std::size_t sid, std::vector<std::size_t> &positions) const
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


void LatticeKTau::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{
    const std::size_t sid = input.GetTranslationId();
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // score phrases wrt how they permute the input (this uses the lattice input and ignores word alignment)
    std::vector<std::size_t> positions(GetInputPositions(inputPath, m_sstate_fname));
    SetKTauInternalToPhrase(scores, ComputeExpectation(sid, positions));
    
    // score phrases wrt how they permute the input given word alignment information (this rearranges the lattice input in target word order)
    // unaligned words behave according to the option of unfold heuristic
    std::vector<std::size_t> permutation(GetPermutation(positions, targetPhrase.GetAlignTerm(), m_unfold_heuristic));
    SetKTauInternalToPhraseGivenWA(scores, ComputeExpectation(sid, permutation));

    // compute distortion cost internal to phrase (this uses the lattice input and ignores word alignment)
    SetInternalDistortionCost(scores, ComputeDistortionCost(positions));
    
    // compute distortion cost internal to phrase given word alignment information (again lattice input is rearranged in target word order)
    SetInternalDistortionCostGivenWA(scores, ComputeDistortionCost(permutation));

    // update feature vector 
    scoreBreakdown.PlusEquals(this, scores);

    // note that this FF could also account for features such as monotone/swap/discontiguous

    // TODO: update estimatedFutureScore?
}


FFState* LatticeKTau::EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
      ScoreComponentCollection* accumulator) const
{
    const std::size_t sid = hypo.GetInput().GetTranslationId();
    // here we need to score the hypothesis wrt yet untranslated words
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    float expectation = 0.0;
    
    // first we need to get information from the input path that the current phrase covers
    const TranslationOption& topt = hypo.GetTranslationOption();
    const InputPath& path = topt.GetInputPath(); 
    // from the arcs of the lattice we get an input permutation
    std::vector<std::size_t> positions = GetInputPositions(path, m_sstate_fname);
    // here we rearrange the words in target word order via word alignment information 
    std::vector<std::size_t> permutation(GetPermutation(positions, topt.GetTargetPhrase().GetAlignTerm(), m_unfold_heuristic));
    
    // now we can update the current coverage vector
    const LatticeKTauState* prev = dynamic_cast<const LatticeKTauState*>(prev_state);  // previous coverage
    WordsBitmap coverage(prev->GetCoverage());
    for (std::size_t i = 0; i < positions.size(); ++i)
        coverage.SetValue(positions[i], true);  

    // untranslated words will later appear to the right of the current phrase (in target word order)
    for (std::size_t right = 0; right < coverage.GetSize(); ++right) {
        if (coverage.GetValue(right))  // translated words have already been scored
            continue;
        for (std::size_t i = 0; i < positions.size(); ++i) { 
            const std::size_t left = positions[i];
            expectation += GetExpectation(sid, left, right);
        }
    }
    
    // we update the score external to the phrase (notice that this feature never depends on word alignment)
    // because the internal order of the phrase does not matter
    SetKTauExternalToPhrase(scores, expectation);

    // compute distortion cost external to the phrase assuming the input permutation (that of the lattice)
    SetExternalDistortionCost(scores, ComputeDistortionCost(prev->GetLastCovered(), positions.front()));
    
    // compute disttortion cost external to the phrase assuming target word order (by rearranging lattice input using word alignment information) 
    SetExternalDistortionCostGivenWA(scores, ComputeDistortionCost(prev->GetLastCoveredGivenWA(), permutation.front()));

    accumulator->PlusEquals(this, scores);
   
    // and return the new coverage vector  
    return new LatticeKTauState(coverage, positions.back(), permutation.back());
}



}

