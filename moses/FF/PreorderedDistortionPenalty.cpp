#include "moses/FF/PreorderedDistortionPenalty.h"

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

PreorderedDistortionPenalty::PreorderedDistortionPenalty(const std::string &line)
  :StatefulFeatureFunction(2, line)
{
    ReadParameters();
    // sanity checks
    UTIL_THROW_IF2(m_mapping_path.empty(),
          "PreorderedDistortionPenalty requires a mapping from s' to s (mapping=<path>).");
}

/**
 * CONFIG FF
 */

void PreorderedDistortionPenalty::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "mapping") {
        if (FileExists(value)) {
            m_mapping_path = value;
        } else {
            UTIL_THROW2("Permutation file not found: " + value);
        }
    } else {
        FeatureFunction::SetParameter(key, value);
    }
}

/**
 * LOAD FILES
 */


void PreorderedDistortionPenalty::Load() 
{
    // read permutations
    ReadPermutations(m_mapping_path);
}

void PreorderedDistortionPenalty::ReadPermutations(const std::string& path) 
{
    std::ifstream fin(path.c_str());
    std::string line;
    while (std::getline(fin, line)) {
        const std::vector<std::string> tokens = Tokenize(line);
        std::vector<std::size_t> permutation(tokens.size());
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            permutation[i] = std::atoi(tokens[i].c_str());  
        }
        m_permutations.push_back(permutation);
    }
}

/**
 * COMPUTE FF
 */
  
void PreorderedDistortionPenalty::InitializeForInput(InputType const& source) 
{
    const std::size_t sid = source.GetTranslationId();  // the sequential id of the segment being translated
    // sanity checks
    UTIL_THROW_IF2(sid >= m_permutations.size(),
          "PreorderedDistortionPenalty::InitializeForInput: it seems like you are missing entries in the table of permutations.");
    UTIL_THROW_IF2(source.GetSize() > m_permutations[sid].size(),
          "PreorderedDistortionPenalty::InitializeForInput: it seems like there is a mismatch in length between input and permutation.");

}


std::vector<std::size_t> PreorderedDistortionPenalty::MapPositions(const std::size_t sid, const WordsRange& range) const
{
    std::vector<std::size_t> permutation(range.GetEndPos() - range.GetStartPos() + 1, 0);
    std::size_t i = 0;
    for (std::size_t pos = range.GetStartPos(); pos <= range.GetEndPos(); ++pos, ++i) {
        permutation[i] = m_permutations[sid][pos];
    }
    return permutation;
}

void PreorderedDistortionPenalty::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{
    const std::size_t sid = input.GetTranslationId();
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // score phrases wrt how they permute the input (this uses the lattice input and ignores word alignment)
    std::vector<std::size_t> positions(MapPositions(sid, inputPath.GetWordsRange()));
    
    // compute distortion cost internal to phrase
    SetInternalDistortionCost(scores, ComputeDistortionCost(positions));
    
    // score phrases wrt how they permute the input given word alignment information (this rearranges the lattice input in target word order)
    // unaligned words behave according to the option of unfold heuristic
    //std::vector<std::size_t> permutation(GetPermutation(positions, targetPhrase.GetAlignTerm(), m_unfold_heuristic));

    
    // compute distortion cost internal to phrase given word alignment information (again lattice input is rearranged in target word order)
    //SetInternalDistortionCostGivenWA(scores, ComputeDistortionCost(permutation));

    // update feature vector 
    scoreBreakdown.PlusEquals(this, scores);

}


FFState* PreorderedDistortionPenalty::EvaluateWhenApplied(const Hypothesis& hypo, const FFState* prev_state, 
      ScoreComponentCollection* accumulator) const
{
    const std::size_t sid = hypo.GetInput().GetTranslationId();
    // here we need to score the hypothesis wrt yet untranslated words
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    
    // first we need to get information from the input path that the current phrase covers
    const TranslationOption& topt = hypo.GetTranslationOption();
    const InputPath& path = topt.GetInputPath(); 
    // the input path implies a permutation
    std::vector<std::size_t> positions = MapPositions(sid, path.GetWordsRange());
    // here we rearrange the words in target word order via word alignment information 
    //std::vector<std::size_t> permutation(GetPermutation(positions, topt.GetTargetPhrase().GetAlignTerm(), m_unfold_heuristic));
    
    // now we can update the current coverage vector
    const PreorderedDistortionPenaltyState* prev = dynamic_cast<const PreorderedDistortionPenaltyState*>(prev_state);  // previous coverage
    
    // compute distortion cost external to the phrase assuming the input permutation (that of the lattice)
    SetExternalDistortionCost(scores, ComputeDistortionCost(prev->GetLastCovered(), positions.front()));
    
    // compute disttortion cost external to the phrase assuming target word order (by rearranging lattice input using word alignment information) 
    //SetExternalDistortionCostGivenWA(scores, ComputeDistortionCost(prev->GetLastCoveredGivenWA(), permutation.front()));

    accumulator->PlusEquals(this, scores);
   
    // and return the new coverage vector  
    //return new PreorderedDistortionPenaltyState(coverage, positions.back(), permutation.back());
    return new PreorderedDistortionPenaltyState(positions.back());
}



}


