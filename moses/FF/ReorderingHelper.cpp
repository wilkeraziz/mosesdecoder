#include "moses/FF/ReorderingHelper.h"

#include <algorithm>
#include <deque>
#include <set>
#include <fstream>
#include <iostream>
#include "moses/Util.h"
#include "moses/ScoreComponentCollection.h"

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

} // anonymous namespace

namespace Moses
{

/** 
 * IO STUFF
 */


void ReorderingHelper::ReadLengthInfo(const std::string& path, std::vector<std::size_t>& lengths)
{
    lengths.clear();
    std::ifstream fin(path.c_str());
    std::string line;
    while (std::getline(fin, line)) {
        try {
            const std::size_t length = Scan<std::size_t>(line);
            lengths.push_back(length);
        } catch (...) {
            UTIL_THROW2("Line " + SPrint(lengths.size() + 1) + ". Problem parsing length information: " + line)
        }
    }
}


void ReorderingHelper::ReadSkipBigramTables(const std::string& path, 
        std::vector< std::map< std::size_t, std::map<std::size_t, double> > >& taus)
{
    taus.clear();
    std::ifstream fin(path.c_str());
    std::string line;
    while (std::getline(fin, line)) {
        const std::vector<std::string> tokens = Tokenize(line, " \t:");
        taus.resize(taus.size() + 1);
        std::map< std::size_t, std::map<std::size_t, double> > &table = taus.back();
        UTIL_THROW_IF2(tokens.size() % 3 != 0,
          "Line " + SPrint(taus.size()) + ". Expected triplets i:j:w, got: " + line);
        if (tokens.size() > 0) {
            for (std::size_t i = 0; i < tokens.size() - 2; i += 3) {
                try {
                    const std::size_t a = Scan<std::size_t>(tokens[i]);
                    const std::size_t b = Scan<std::size_t>(tokens[i + 1]);
                    const double weight = Scan<double>(tokens[i + 2]);
                    table[a][b] = weight;
                } catch (...) {
                    UTIL_THROW2("Line " + SPrint(taus.size()) + ". Problem parsing triplet " + tokens[i] + ":" + tokens[i + 1] + ":" + tokens[i + 2]);
                }
            }
        }
    }
}

void ReorderingHelper::ReadPermutations(const std::string& path, std::vector< std::vector<std::size_t> >& permutations) 
{
    permutations.clear();
    std::ifstream fin(path.c_str());
    std::string line;
    while (std::getline(fin, line)) {
        const std::vector<std::string> tokens = Tokenize(line);
        permutations.resize(permutations.size() + 1);
        std::vector<std::size_t> &permutation = permutations.back();
        permutation.resize(tokens.size());
        for (std::size_t i = 0; i < tokens.size(); ++i) {
            try {
                permutation[i] = Scan<std::size_t>(tokens[i]);
            } catch (...) {
                UTIL_THROW2("Line " + SPrint(permutations.size()) + ". Problem parsing 0-based position " + tokens[i]);
            }
        }
    }
}

/**
 * MAPPING FROM S' TO S
 */

std::size_t ReorderingHelper::MapInputPosition(const std::vector< std::vector<std::size_t> >& permutations,
        const std::size_t sid, const std::size_t i) 
{
    return (permutations.empty())? i : permutations[sid][i]; 
}

std::vector<std::size_t> ReorderingHelper::MapInputPositions(const std::vector< std::vector<std::size_t> >& permutations,
       const std::size_t sid, 
       const std::size_t offset,
       const std::size_t size) 
{
    std::vector<std::size_t> positions(size);
    for (std::size_t f = 0; f < positions.size(); ++f) {
        positions[f] = ReorderingHelper::MapInputPosition(permutations, sid, offset + f);
    }
    return positions;
}

std::vector<std::size_t> ReorderingHelper::MapInputPositions(const std::vector< std::vector<std::size_t> >& permutations,
       const std::size_t sid, 
       const WordsRange &range) 
{
    return ReorderingHelper::MapInputPositions(permutations, sid, range.GetStartPos(), range.GetNumWordsCovered());
}

std::vector<std::size_t> ReorderingHelper::GetInputPositions(const WordsRange &currWordRange) 
{
    const std::size_t offset = currWordRange.GetStartPos();
    std::vector<std::size_t> positions(currWordRange.GetNumWordsCovered());
    for (std::size_t f = 0; f < positions.size(); ++f) {
        positions[f] = offset + f;
    }
    return positions;
}

/*
 * TARGET PERMUTATIONS
 */
    
std::vector<std::size_t> ReorderingHelper::GetPermutation(const std::vector<std::size_t> &input,
        const AlignmentInfo& ainfo,
        const char targetWordOrderHeuristic) 
{
    // these are 1-1 alignments such that
    // alignment[i] points to a unique target word (0-based) corresponding to the ith (0-based) source word
    // to obtain such a correspondence we apply a heuristic (see below)
    std::vector<int> alignment(input.size());
    
    if (targetWordOrderHeuristic == 'M') { // ignores alignment information and returns the trivial permutation
        return input;
    } else {  // uses word alignment info to permute input as per target word-order
        if (targetWordOrderHeuristic == 'L') { // this process depends on a heuristic to deal with unaligned words
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

/*
 * EXPECTATIONS
 */

double ReorderingHelper::GetExpectation(const std::vector< std::map< std::size_t, std::map<std::size_t, double> > > &taus,
        const std::size_t sid, const std::size_t left, const std::size_t right, const float missing)
{
    std::map< std::size_t, std::map<std::size_t, double> >::const_iterator it = taus[sid].find(left);
    if (it == taus[sid].end()) 
        return missing;
    std::map<std::size_t, double>::const_iterator it2 = it->second.find(right);
    if (it2 == it->second.end())
        return missing;
    return it2->second;
}

float ReorderingHelper::ComputeExpectation(const std::vector< std::map< std::size_t, std::map<std::size_t, double> > >& taus,
        const std::size_t sid, std::vector<std::size_t> &positions, const float missing)
{
    float weight = 0.0;
    for (std::size_t i = 0; i < positions.size() - 1; ++i) {
        const std::size_t left = positions[i];
        for (std::size_t j = i + 1; j < positions.size(); ++j) {
            const std::size_t right = positions[j];
            weight += GetExpectation(taus, sid, left, right, missing);
        }
    }
    return weight;
}

float ReorderingHelper::ComputeExpectation(const std::vector< std::map< std::size_t, std::map<std::size_t, double> > >& taus,
        const std::size_t sid, const std::vector<std::size_t>& positions, const WordsBitmap& coverage, const float missing) 
{
    float weight = 0.0;
    for (std::size_t right = 0; right < coverage.GetSize(); ++right) {
        if (coverage.GetValue(right))  // translated words have already been scored
            continue;
        for (std::size_t i = 0; i < positions.size(); ++i) { 
            const std::size_t left = positions[i];
            weight += GetExpectation(taus, sid, left, right, missing);
        }
    }
    return weight;
}

float ReorderingHelper::ComputeExpectation(const std::vector< std::map< std::size_t, std::map<std::size_t, double> > >& taus,
        const std::size_t sid, 
        const std::vector<std::size_t>& left, 
        const std::vector<std::size_t>& right,
        const float missing) 
{
    float weight = 0.0;
    for (std::vector<std::size_t>::const_iterator l = left.begin(); l != left.end(); ++l) {
        for (std::vector<std::size_t>::const_iterator r = right.begin(); r != right.end(); ++r) {
            weight += GetExpectation(taus, sid, *l, *r, missing);
        }
    }
    return weight;
}

/*
 * DISTORTION COST
 */

int ReorderingHelper::ComputeDistortionCost(const std::size_t left, const std::size_t right)
{
    int jump = right - left - 1;
    return (jump < 0)? -jump : jump;
}

int ReorderingHelper::ComputeDistortionCost(const std::vector<std::size_t> &positions)
{
    if (positions.empty())
        return 0;
    int d = 0;
    int last_covered = positions.front();
    for (std::size_t i = 1; i < positions.size(); ++i) {
        d += ReorderingHelper::ComputeDistortionCost(last_covered, positions[i]);
        last_covered = positions[i];
    }
    return d;
}

/*
 * LATTICE INPUT PATH STUFF
 *
 */

std::vector<std::size_t> ReorderingHelper::GetInputPositionsFromArcs(const InputPath &inputPath, const std::string& key) 
{
    // retrieve path pointers
    std::deque<int> accstates;
    const InputPath *p = &inputPath;
    while(p != NULL) {
        const ScorePair *s = p->GetInputScore();
        UTIL_THROW_IF2(s == NULL,
              "ReorderingHelper::GetInputPositionsFromArcs: an input path returned a null score.");
        std::map<StringPiece, float>::const_iterator it = s->sparseScores.find(key);
        UTIL_THROW_IF2(it == s->sparseScores.end(),
              "ReorderingHelper::GetInputPositionsFromArcs: an input arc misses the sstate feature.");
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


} // namespace Moses

