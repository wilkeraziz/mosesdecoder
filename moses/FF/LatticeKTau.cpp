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
#include "LatticeKTau.h"
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

bool IsAligned(int i)
{
    return (i >= 0);
}

bool CompareAlignment(const std::pair<std::size_t, std::size_t>& a, const std::pair<std::size_t, std::size_t>& b)
{
    return (a.second < b.second);
}

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


std::vector<std::size_t> LatticeKTau::GetInputPositions(const InputPath &inputPath) const
{
    // retrieve path pointers
    std::deque<int> accstates;
    const InputPath *p = &inputPath;
    while(p != NULL) {
        const ScorePair *s = p->GetInputScore();
        UTIL_THROW_IF2(s == NULL,
              "LatticeKTau::GetSState: an input path returned a null score.");
        std::map<StringPiece, float>::const_iterator it = s->sparseScores.find(m_sstate_fname);
        UTIL_THROW_IF2(it == s->sparseScores.end(),
              "LatticeKTau::GetSState: an input arc misses the sstate feature.");
        int acc = int(it->second);
        accstates.push_front(acc);
        p = p->GetPrevPath();
    }
    // restore positions from accumulated values
    std::vector<std::size_t> positions(accstates.size());
    if (accstates.empty())
        return positions;
    std::size_t prev = accstates.front();
    positions[0] = prev;
    std::deque<int>::iterator it = accstates.begin();
    std::size_t i = 1;
    for (++it; it != accstates.end(); ++it, ++i) {
        positions[i] = *it - prev;
        prev = *it;
    }
    return positions;
}


std::vector<std::size_t> LatticeKTau::GetPermutation(const std::vector<std::size_t> &input,
        const AlignmentInfo& ainfo) const
{
    //const std::size_t offset = currWordRange.GetStartPos();
    //const AlignmentInfo& ainfo = targetPhrase.GetAlignTerm();
    
    // these are 1-1 alignments such that
    // alignment[i] points to a unique target word (0-based) corresponding to the ith (0-based) source word
    // to obtain such a correspondence we apply a heuristic (see below)
    std::vector<int> alignment(input.size());
    
    if (m_unfold_heuristic == 'M') { // ignores alignment information and returns the trivial permutation
        return input;
    } else {  // uses word alignment info to permute input as per target word-order
        if (m_unfold_heuristic == 'L') { // this process depends on a heuristic to deal with unaligned words
            int left = -1;
            for (std::size_t f = left + 1; f < alignment.size(); ++f) {
                std::set<std::size_t> es = ainfo.GetAlignmentsForSource(f);
                if (es.empty()) {
                    alignment[f] = left;
                } else {
                    alignment[f] = *es.begin();  // leftmost target word
                    left = alignment[f];
                }
            }
            // look for the first aligned word
            std::vector<int>::iterator first = std::find_if(alignment.begin(), alignment.end(), IsAligned);
            if (first != alignment.end()) {  // if we find any
                // we borrow its alignment point
                for (std::vector<int>::iterator u = alignment.begin(); u != first; ++u){
                    *u = *first;
                }
            }
        } else {
            int right = -1;  //alignment.size();
            for (int f = alignment.size() - 1; f >= 0; --f) {
                std::set<std::size_t> es = ainfo.GetAlignmentsForSource(f);
                if (es.empty()) {
                    alignment[f] = right;
                } else {
                    alignment[f] = *es.begin();  // leftmost target word
                    right = alignment[f];
                }
            }
            // look for the last aligned word
            std::vector<int>::reverse_iterator last = std::find_if(alignment.rbegin(), alignment.rend(), IsAligned);
            if (last != alignment.rend()) { // if we find any
                // we borrow its alignment point
                for (std::vector<int>::reverse_iterator u = alignment.rbegin(); u != last; ++u){
                    *u = *last;
                }
            }
        }
    }
    // find alignment
    std::vector< std::pair<std::size_t, std::size_t> > pairs(alignment.size());
    for (std::size_t f = 0; f < alignment.size(); ++f) {
        pairs[f] = std::pair<std::size_t, std::size_t>(f, alignment[f]);
    }
    std::stable_sort(pairs.begin(), pairs.end(), CompareAlignment);

    std::vector<std::size_t> permutation(alignment.size());
    for (std::size_t f = 0; f < pairs.size(); ++f) {
        permutation[f] = input[pairs[f].first];
    }

    return permutation;
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
    std::vector<std::size_t> positions(GetInputPositions(inputPath));
    SetKTauInternalToPhrase(scores, ComputeExpectation(sid, positions));
    
    // score phrases wrt how they permute the input given word alignment information (this rearranges the lattice input in target word order)
    // unaligned words behave according to the option of unfold heuristic
    std::vector<std::size_t> permutation(GetPermutation(positions, targetPhrase.GetAlignTerm()));
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
    std::vector<std::size_t> positions = GetInputPositions(path);
    // here we rearrange the words in target word order via word alignment information 
    std::vector<std::size_t> permutation(GetPermutation(positions, topt.GetTargetPhrase().GetAlignTerm()));
    
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

