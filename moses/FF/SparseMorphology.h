#pragma once

#include <vector>
#include <set>
#include <string>
#include <sstream>
#include "moses/FF/StatelessFeatureFunction.h"
#include "moses/InputPath.h"
#include "moses/Hypothesis.h"
#include "moses/ChartHypothesis.h"
#include "moses/TypeDef.h"
#include "util/string_piece.hh"

namespace Moses
{


/*
 * Sparse features related to morphology. In general the features are made of string features extracted from input/output factors.
 * SparseMorphology does not produce actual useable features, it cannot even be instantiated by the decoder.
 * However, other FFs inherit from SparseMorphology. They all support at least the following set of arguments.
 *
 * Arguments:
 *  input-factor=<int>  (default: 0)
 *      which factor to extract from source phrases
 *      Typical use: morphology tag factor
 *  output-factor=<int>  (default: 0)
 *      which factor to extract from target phrases
 *      Typical use: surface factor
 *  input-max-chars=<int>  (default: 0)
 *      how long can the feature be in chars
 *          0 means no limit
 *          n>0 means a prefix up to n chars
 *          n<0 means a suffix up to n chars
 *      Typical use: 0 (whole tag), or small positive n when tags contain attributes from general to specific (discards rare attributes)
 *  output-max-chars=<int>  (default: 0)
 *      same as input-max-chars but applies to output factor
 *      Typical use: -2 (or -n with small positive n) to consider just a suffix (for languages that mark with suffixes)
 *          2 (or small positive n) to consider just a prefix (for languages that mark with prefixes)
 *  feature-vocab=<path>
 *      constrain the vocabulary of features
 *      Format: the file must contain one feature per line (possibly paired with a weight which will be ignored)
 *      Typical use: control the number of sparse features
 *  oov-feature=<str>  (default: None)
 *      used in combination with feature-vocab to rewrite an OOV feature; if this is an empty string, the feature is simply not fired
 *      Format: a non-empty string without empty spaces
 *      Typical use: debugging perhaps?
 *  ignore-prefix-in-feature-vocab=true|false  (default: false)
 *      if true, assumes features in feature-vocab are prefixed by a component's name which should be ignored when reading entries from feature-vocab
 *      Typical use: one wants to reuse a weight file to constrain the vocabulary, however, in weight files features are prefixed as in MyFF_FeatureName
 *  input-vocab=<path>
 *      constrain the vocabulary of input symbols, when a symbol is not in vocab it gets replaced by input-oov (see below)
 *      Format: one symbol per line, no prefix, no weights
 *      Typical use: reduce the number of combinations
 *  input-oov=<str>  (default: *)
 *      used in combination with input-vocab to rewrite OOV symbols.
 *      Format: a string without empty spaces
 *  output-vocab=<path>
 *      constrain the vocabulary of output symbols, when a symbol is not in vocab it gets replaced by output-oov (see below)
 *      Format: one symbol per line, no prefix, no weights
 *      Typical use: select strings likely to relate to useful morphemes
 *  input-oov=<str>  (default *)
 *      used in combination with output-vocab to rewrite OOV symbols.
 *      Format: a string without empty spaces
 *  language-separator=<str>  (default: =>)
 *      which symbol to use when concatenating input and output symbols
 *      Format: non-empty string whitout white spaces
 *
 */
class SparseMorphology : public StatelessFeatureFunction
{
protected:
    FactorType m_input_factor;
    FactorType m_output_factor;
    std::size_t m_input_max_chars;
    std::size_t m_output_max_chars;
    bool m_input_reverse;
    bool m_output_reverse;
    bool m_constrained_vocab;
    std::string m_vocab_path;
    bool m_ignore_prefix;
    std::string m_language_separator;
    std::set<std::string> m_features;
    // constraining input/output vocabulary 
    bool m_constrained_input_vocab;
    bool m_constrained_output_vocab;
    std::string m_input_vocab_path;
    std::string m_output_vocab_path;
    std::set<std::string> m_input_vocab;
    std::set<std::string> m_output_vocab;
    std::string m_missing_feature;
    std::string m_input_oov;
    std::string m_output_oov;

    // return a string representation for an input symbol (constraints may apply)
    inline std::string GetInputSymbol(const std::string& str) const
    {
        return (!m_constrained_input_vocab || m_input_vocab.find(str) != m_input_vocab.end())? str : m_input_oov;
    }
    
    // return a string representation for an output symbol (constraints may apply)
    inline std::string GetOutputSymbol(const std::string& str) const
    {
        return (!m_constrained_output_vocab || m_output_vocab.find(str) != m_output_vocab.end())? str : m_output_oov;
    }
   
    // return the feature name (possibly prefixed with its component name) given the complete feature representation (constraints may apply)
    bool FireFeature(ScoreComponentCollection& scores, const std::string& feature) const;

    // apply factor/size/reverse to a word
    static StringPiece GetPiece(const Word& word, const FactorType factor, const std::size_t size, bool reverse);

    // apply factor/size/rever to a phrase (word by word)
    static std::vector<StringPiece> GetPieces(const Phrase& phrase, const FactorType factor, const std::size_t size, bool reverse);

    // read a file of features
    static void ReadVocab(const std::string& path, std::set<std::string>& features, bool ignore_prefix);
    
    inline static std::string StringPieceToString(const StringPiece &sp)
    {
        std::stringstream ss;
        ss << sp;
        return ss.str();
    }
    
    SparseMorphology(const std::string &line);

public:

    virtual ~SparseMorphology() {}

    bool IsUseable(const FactorMask &mask) const {
        return false;
    }

    void Load();

    void EvaluateInIsolation(const Phrase &source
                           , const TargetPhrase &targetPhrase
                           , ScoreComponentCollection &scoreBreakdown
                           , ScoreComponentCollection &estimatedFutureScore) const
    {
    }

    void EvaluateInIsolation(const InputPath &inputPath
                           , const TargetPhrase &targetPhrase
                           , ScoreComponentCollection &scoreBreakdown
                           , ScoreComponentCollection &estimatedFutureScore) const
    {
    }

    void EvaluateTranslationOptionListWithSourceContext(const InputType &input
      , const TranslationOptionList &translationOptionList) const
    {
    }


    virtual void EvaluateWithSourceContext(const InputType &input
                                 , const InputPath &inputPath
                                 , const TargetPhrase &targetPhrase
                                 , const StackVec *stackVec
                                 , ScoreComponentCollection &scoreBreakdown
                                 , ScoreComponentCollection *estimatedFutureScore = NULL) const
    {
    }

    void EvaluateWhenApplied(const Hypothesis& hypo,
                           ScoreComponentCollection* accumulator) const
    {
    }

    void EvaluateWhenApplied(const ChartHypothesis &hypo,
                           ScoreComponentCollection* accumulator) const
    {
    }


    void SetParameter(const std::string& key, const std::string& value);

};

}



