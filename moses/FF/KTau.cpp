#include <cstdlib>
#include <algorithm>
#include <vector>
#include <set>
#include <fstream>
#include <iostream>
#include "moses/WordsBitmap.h"
#include "moses/WordsRange.h"
#include "moses/Hypothesis.h"
#include "KTau.h"
#include "moses/ScoreComponentCollection.h"
#include "moses/TargetPhrase.h"
#include "moses/InputType.h"
#include "moses/Util.h"
#include "moses/AlignmentInfo.h"
#include "moses/InputPath.h"

using namespace std;

namespace 
{

bool IsAligned(int i)
{
    return (i >= 0);
}

bool CompareAlignment(const std::pair<std::size_t, std::size_t>& a, const std::pair<std::size_t, std::size_t>& b)
{
    return (a.second < b.second);
}

} 

namespace Moses
{

 KTau::KTau(const std::string &line)
  :StatelessFeatureFunction(2, line), m_unfold_heuristic('L')
{
    ReadParameters();
    // sanity checks
    UTIL_THROW_IF2(m_table_path.empty(),
          "Expected Kendall tau feature requires a table of expectations of skip-bigrams");
}

/**
 * CONFIG FF
 */

void KTau::SetParameter(const std::string& key, const std::string& value)
{
  if (key == "table") {
        // read table of skip-bigram probabilities
        if (FileExists(value)) {
            m_table_path = value;
        } else {
            UTIL_THROW2("Expectation file not found: " + value);
        }
  } else if (key == "mapping") {
        if (FileExists(value)) {
            m_mapping_path = value;
        } else {
            UTIL_THROW2("Permutation file not found: " + value);
        }
  } else if (key == "unfold") {
      if (value == "monotone")
          m_unfold_heuristic = 'M';
      else if (value == "left") 
          m_unfold_heuristic = 'L';
      else if (value == "right") 
          m_unfold_heuristic = 'R';
      else
          UTIL_THROW2("Unknown heuristic: " + value);
  } else {
        StatelessFeatureFunction::SetParameter(key, value);
  }
}

void KTau::Load() 
{
    // read expectations
    ReadExpectations(m_table_path);
    // read table of permutations
    if (!m_mapping_path.empty())
        ReadPermutations(m_mapping_path);
}

void KTau::ReadExpectations(const std::string& path) 
{
    std::ifstream fin(path.c_str());
    std::string line;
    while (std::getline(fin, line)) {
        const std::vector<std::string> tokens = Tokenize(line, " \t:");
        std::map< std::size_t, std::map<std::size_t, double> > taus;
        for (std::size_t i = 0; i < tokens.size() - 2; i += 3) {
            const std::size_t a = std::atoi(tokens[i].c_str());
            const std::size_t b = std::atoi(tokens[i + 1].c_str());
            const double expectation = std::atof(tokens[i + 2].c_str());
            taus[a][b] = expectation; 
        }
        m_taus.push_back(taus);
    }
}

void KTau::ReadPermutations(const std::string& path) 
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
  
void KTau::InitializeForInput(InputType const& source) 
{
    m_seg = source.GetTranslationId();  // the sequential id of the segment being translated
}

std::size_t KTau::MapInputPosition(const std::size_t i) const
{
    return (m_permutations.empty())? i : m_permutations[m_seg][i]; 
}

double KTau::GetExpectation(const std::size_t left, const std::size_t right) const
{
    std::map< std::size_t, std::map<std::size_t, double> >::const_iterator it = m_taus[m_seg].find(left);
    if (it == m_taus[m_seg].end()) 
        return 0.0;
    std::map<std::size_t, double>::const_iterator it2 = it->second.find(right);
    if (it2 == it->second.end())
        return 0.0;
    return it2->second;
}

std::vector<std::size_t> KTau::GetPermutation(const WordsRange& currWordRange,
        const TargetPhrase& targetPhrase) const
{
    const std::size_t offset = currWordRange.GetStartPos();
    
    // these are 1-1 alignments such that
    // alignment[i] points to a unique target word (0-based) corresponding to the ith (0-based) source word
    // to obtain such a correspondence we apply a heuristic (see below)
    std::vector<int> alignment(currWordRange.GetNumWordsCovered());
    
    if (m_unfold_heuristic == 'M') { // ignores alignment information and returns the trivial permutation
        std::vector<std::size_t> permutation(currWordRange.GetNumWordsCovered());
        for (std::size_t f = 0; f < permutation.size(); ++f) {
            permutation[f] = f + offset;
        }
        return permutation;
    } else{
        const AlignmentInfo& ainfo = targetPhrase.GetAlignTerm();
        if (m_unfold_heuristic == 'L') {
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
        permutation[f] = pairs[f].first + offset;
    }

    return permutation;
}


void KTau::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{
    const std::vector<std::size_t> sequence(GetPermutation(inputPath.GetWordsRange(), targetPhrase));
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    float expectation = 0.0;
    for (std::size_t i = 0; i < sequence.size() - 1; ++i) {
        const std::size_t a = sequence[i];
        for (std::size_t j = i + 1; j < sequence.size(); ++j) {
            const std::size_t b = sequence[j];
            expectation += GetExpectation(MapInputPosition(a), MapInputPosition(b));
        }
    }
    SetKTauInternalToPhrase(scores, expectation);
    scoreBreakdown.PlusEquals(this, scores);
}

void KTau::EvaluateWhenApplied(const Hypothesis& hypo,
    ScoreComponentCollection* accumulator) const
{
    const WordsBitmap& bmap = hypo.GetWordsBitmap();
    const WordsRange& curr = hypo.GetCurrSourceWordsRange();
    // isolate untranslated words as those will necessarily appear to the right of the current phrase
    std::vector<std::size_t> future; 
    future.reserve(bmap.GetSize() - bmap.GetNumWordsCovered());
    // collect information from coverage vector
    for (std::size_t i = 0; i < curr.GetStartPos(); ++i) { 
        if (!bmap.GetValue(i)) 
            future.push_back(i);
    }
    for (std::size_t i = curr.GetEndPos() + 1; i < bmap.GetSize(); ++i) {
        if (!bmap.GetValue(i))
            future.push_back(i);
    }
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    float expectation = 0.0;
    for (std::size_t left = curr.GetStartPos(); left <= curr.GetEndPos(); ++left) {
        for (std::size_t j = 0; j < future.size(); ++j) {
            const std::size_t right = future[j];
            expectation += GetExpectation(MapInputPosition(left), MapInputPosition(right));
        }
    }
    SetKTauExternalToPhrase(scores, expectation);
    accumulator->PlusEquals(this, scores);
}

void KTau::EvaluateWhenApplied(const ChartHypothesis &hypo,
    ScoreComponentCollection* accumulator) const
{
    /*
    const WordsRange& curr = hypo.GetCurrSourceRange();
    const TargetPhrase& ephr = hypo.GetCurrTargetPhrase();
    //std::clog << "range=" << curr << std::endl;
    if (hypo.GetTranslationOption().GetInputPath()->GetPrevPath() == NULL) { // basic
    } else { // concatenation
        const WordsRange& prev = hypo.GetTranslationOption().GetInputPath()->GetPrevPath()->GetWordsRange();
        int start = prev.GetStartPos() - curr.GetStartPos();
        int end = prev.GetEndPos() - curr.GetEndPos();
        //std::clog << "start=" << start << " end=" << end << std::endl;
    }

    std::clog << std::endl;
    accumulator->PlusEquals(this, 0.0);
    // I need the analog of bmap

    //accumulator->PlusEquals(this, 0.0);
    */
}




}

