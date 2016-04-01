#pragma once

#include <vector>
#include <map>
#include <string>
#include "moses/WordsRange.h"
#include "moses/AlignmentInfo.h"
#include "moses/WordsBitmap.h"
#include "moses/InputPath.h"


namespace Moses
{

/*
 * This implement a bunch of static functions which help reordering FFs 
 * particularly those concerning preordering (text or lattice)
 */
class ReorderingHelper 
{
    
public:

    /*
     * Read the original length of each segment.
     * Format:
     *  1 positive integer per line.
     */
    static void ReadLengthInfo(const std::string& path, std::vector<std::size_t>& lengths);

    /*
     * Load a file of of functions over skip bigrams.
     * Format:
     *   1 function per line
     *      each function is represented as a table of skip-bigrams and their values 
     *          each cell in the table is a triplet i:j:w, cells can be separated by a space or tab
     *              where i and j are 0-based indices and w is the contribution of skip-bigram (i, j)
     *              within a triplet each field is separated by a pace (or a tab, or a colon). 
     */
    static void ReadSkipBigramTables(const std::string& path, 
            std::vector< std::map< std::size_t, std::map<std::size_t, double> > >& taus);
    
    /*
     * Load a file of permutations: mappings from s' to s.
     * This file is necessary when the input segment being translated is itself a permutation (s') 
     * of some original sequence (s).
     * Format: one permutation per line, each associated with an input segment.
     *  A line is a list of 0-based integers, the ith number represents the position of the ith s' token in s. 
     */
    static void ReadPermutations(const std::string& path, 
            std::vector< std::vector<std::size_t> >& permutations);

    /*
     * Return the input position in s associated with an input position in s'.
     * If the table of permutations is empty, assume identity permutations.
     * This method assumes the table is either empty or complete (i.e. one permutation per input segment).
     *
     * Arguments:
     *  permutations: table of permutations
     *  sid: current segment (indicates a row in the table)
     *  i: input position in s'
     */
    static std::size_t MapInputPosition(const std::vector< std::vector<std::size_t> >& permutations,
            const std::size_t sid, const std::size_t i);

    /*
     * Return the input positions associated with a closed interval [begin=offset ... end=offset + size]
     */
    static std::vector<std::size_t> MapInputPositions(const std::vector< std::vector<std::size_t> >& permutations,
            const std::size_t sid, 
            const std::size_t offset,
            const std::size_t size);

    /*
     * Return the input positions associated with a range.
     * This calls MapInputPositions(permutations, sid, offset, size).
     */
    static std::vector<std::size_t> MapInputPositions(const std::vector< std::vector<std::size_t> >& permutations,
            const std::size_t sid, const WordsRange &range);
    
    /*
     * This returns the input path associated with a word range (offset, size).
     * For example (offset=2, size=4) returns [2, 3, 4, 5]
     */
    static std::vector<std::size_t> GetInputPositions(const WordsRange &currWordRange);

    /*
     * Return the permutation associated with a certain input range.
     * The input range is assumed to be over s', thus we take a table of permutations to translate that to a permutation of s.
     * This method calls MapInputPosition to map from s' to s.
     * Arguments:
     *  permutations: table of permutations (either empty or complete, see MapInputPosition)
     *  sid: current segment (indicates a row in the table)
     *  currWordRange: current range
     */
    static std::vector<std::size_t> GetInputPositions(const std::vector< std::vector<std::size_t> >& permutations,
       const std::size_t sid, 
       const WordsRange &currWordRange);

    /*
     * This permutes elements given in source language word order into target language word order.
     * Please select a heuristic to unfold word alignments, that is, to permute the input in target word-order.
     * This heuristic accepts 3 values:
     *  'M' (for montone) means skip it altogether, that is, ignore word alignment information and assume a monotone mapping between input and output
     *  'L' (for left) means consult word alignment information to obtain target word-order and attaches unaligned words to its left
     *  'R' (for right) similar to 'L', but looks for aligned words to the right of the unaligned ones
     */
    static std::vector<std::size_t> GetPermutation(const std::vector<std::size_t> &input, const AlignmentInfo& ainfo, const char targetWordOrderHeuristic);

    /*
     * Read the expectation of (left, right) for sentence sid from a table of taus.
     * The table is assumed to be already log-transformed (if applicable). This method still needs to know about log-transformation
     * and log(0) in order to be consistent with the table whenever we detect a missing entry.
     */
    static double GetExpectation(const std::vector< std::map< std::size_t, std::map<std::size_t, double> > > &taus,
            const std::size_t sid, 
            const std::size_t left, 
            const std::size_t right,
            const float missing);
    
    /*
     * Return the expectation associated with a certain permutations
     * Arguments:
     *  taus: table of skip-bigram expectations
     *  sid: current sentence (indicates a row in the table)
     *  positions: permutation
     *  log_transform: whether or not taus are log-transformed
     *  log_zero: log(0)
     */
    static float ComputeExpectation(const std::vector< std::map< std::size_t, std::map<std::size_t, double> > >& taus,
            const std::size_t sid, std::vector<std::size_t> &permutation,
            const float missing);

    /*
     * Return the expectation of a certain sequence of positions (represeting words already translated) wrt yet untranslated words.
     *
     * Note that yet untranslated words can only appear to the right of the input sequence.
     * This method assumes that the coverage vector has already been updated such that positions in the input sequence appear as "translated"/"covered".
     * Arguments:
     *  taus: table of skip-bigram expectations
     *  sid: current sentence (indicates a row in the table)
     *  positions: words just recently covered
     *  coverage: a bit vector for the whole sentence (where words in positions are marked translated)
     *  log_transform: whether or not taus are log-transformed
     *  log_zero: log(0)
     */
    static float ComputeExpectation(const std::vector< std::map< std::size_t, std::map<std::size_t, double> > >& taus,
            const std::size_t sid, const std::vector<std::size_t>& positions, const WordsBitmap& coverage,
            const float missing);

    /*
     * Return the expectation of concatenating two sequences.
     * Typical use is to have a sequence of words just translated and a sequence of words yet to be translated.
     *
     * Arguments:
     *  taus: table of skip-bigram expectations
     *  sid: current sentence (indicates a row in the table)
     *  left: the first sequence
     *  right: the second sequence
     *  log_transform: whether or not taus are log-transformed
     *  log_zero: log(0)
     */
    static float ComputeExpectation(const std::vector< std::map< std::size_t, std::map<std::size_t, double> > >& taus,
        const std::size_t sid, 
        const std::vector<std::size_t>& left, 
        const std::vector<std::size_t>& right,
        const float missing);

    /*
     * Computes the usual distortion cost incurred when we concatenate two positions.
     * The typical use is to concatenate the last position of the previous phrase (left) and the first position of the current phrase (right)
     *
     * Arguments:
     *  left: the first position
     *  right: the second position
     * Return: 
     *  right - (left + 1)
     */
    static int ComputeDistortionCost(const std::size_t left, const std::size_t right);

    /*
     * Total distortion cost incurred when we concatenate a sequence of positions in the given order.
     * Typical use: cover a certain permutation of s with a source phrase.
     *
     * See this as a reduce operation that sums over ComputeDistortionCost(left, right).
     * Arguments:
     *  positions: a sequence of positions.
     */
    static int ComputeDistortionCost(const std::vector<std::size_t> &positions);

    static std::vector<std::size_t> GetInputPositionsFromArcs(const InputPath &inputPath, const std::string& key);
};

}


