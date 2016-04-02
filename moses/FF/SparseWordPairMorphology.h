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
 * For each word within a phrase pair, this FF creates one or more sparse feature of the kind
 * 
 *  src_symbol => tgt_symbol 
 * where 
 *  src_symbols is an input symbol constructed by SparseMorphology
 *  and tgt_symbol is an output symbol constructed by SparseMorphology
 *  there is one such feature for each target word that aligns to the given source word
 *  special treatment applies if the source word is unaligned (see below)
 *
 * Arguments:
 *  fire-unaligned=true|false  (default: false)
 *      fires src_symbol => unaligned
 *      Typical use: trying to learn which morphological features should not disappear 
 *  unaligned-repr=<str>  (default: <null>)
 *      changes the representation of a null alignment
 *      Typical use: beautifying things?
 *
 */
class SparseWordPairMorphology : public SparseMorphology
{
protected:

    bool m_fire_unaligned;
    std::string m_unaligned_repr;
    
    std::string GetFeature(const StringPiece &f, const StringPiece &e) const;
    
public:
    SparseWordPairMorphology(const std::string &line);

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





