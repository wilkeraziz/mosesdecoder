#pragma once

#include <vector>
#include <map>
#include <string>
#include "moses/FF/StatelessFeatureFunction.h"
#include "moses/InputPath.h"
#include "moses/Hypothesis.h"
#include "moses/ChartHypothesis.h"
#include "moses/TypeDef.h"
#include "util/string_piece.hh"
#include "moses/FF/SparseMorphology.h"

namespace Moses
{

/*
 * For each phrase pair, this FF creates one sparse feature of the kind
 *  src_symbols => tgt_symbols
 * where 
 *  src_symbols is a sequence of input symbols constructed by SparseMorphology
 *  and tgt_symbols is a sequence of output symbols constructed by SparseMorphology
 *
 * Note that word-alignment does not directly affect this feature.
 * 
 * Arguments:
 *  word-separator=<str>  (default: colon)
 *      which symbol to use when concatenating words
 *      Format: non-empty string whitout white spaces
 */
class SparsePhrasePairMorphology : public SparseMorphology
{
protected:

    std::string m_word_separator;
    std::string GetFeature(const std::vector<StringPiece> &fs, const std::vector<StringPiece> &es) const;

public:
  SparsePhrasePairMorphology(const std::string &line);

  bool IsUseable(const FactorMask &mask) const {
    return true;
  }
  
  void EvaluateWithSourceContext(const InputType &input
                                 , const InputPath &inputPath
                                 , const TargetPhrase &targetPhrase
                                 , const StackVec *stackVec
                                 , ScoreComponentCollection &scoreBreakdown
                                 , ScoreComponentCollection *estimatedFutureScore = NULL) const;

    void SetParameter(const std::string& key, const std::string& value);

};

}




