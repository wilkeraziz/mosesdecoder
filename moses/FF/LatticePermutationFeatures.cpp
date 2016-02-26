#include "moses/FF/LatticePermutationFeatures.h"

#include <vector>
#include <deque>
#include <map>
#include <set>
#include <algorithm>

#include "moses/ScoreComponentCollection.h"
#include "moses/Util.h"

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

} 


namespace Moses
{

std::vector<std::size_t> GetInputPositions(const InputPath &inputPath, const std::string& key) 
{
    // retrieve path pointers
    std::deque<int> accstates;
    const InputPath *p = &inputPath;
    while(p != NULL) {
        const ScorePair *s = p->GetInputScore();
        UTIL_THROW_IF2(s == NULL,
              "GetInputPositions: an input path returned a null score.");
        std::map<StringPiece, float>::const_iterator it = s->sparseScores.find(key);
        UTIL_THROW_IF2(it == s->sparseScores.end(),
              "GetInputPositions: an input arc misses the sstate feature.");
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


std::vector<std::size_t> GetPermutation(const std::vector<std::size_t> &input,
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


} // namespace Moses

