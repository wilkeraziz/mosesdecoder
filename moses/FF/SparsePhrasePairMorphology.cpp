#include "moses/FF/SparsePhrasePairMorphology.h"
#include "moses/Util.h"
#include <sstream>

using namespace std;

namespace Moses
{

 SparsePhrasePairMorphology::SparsePhrasePairMorphology(const std::string &line)
  :SparseMorphology(line), m_word_separator(":")
{
    ReadParameters();
}

void SparsePhrasePairMorphology::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "word-separator") {
        m_word_separator = value;
    } else {
        SparseMorphology::SetParameter(key, value);
    }
}

std::string SparsePhrasePairMorphology::GetFeature(const std::vector<StringPiece> &fs, const std::vector<StringPiece> &es) const
{
    std::stringstream ss;
    if (!m_constrained_input_vocab) // unconstrained: faster version 
        ss << Join(m_word_separator, fs);
    else {  // here we need to check the pieces one by one
        std::vector<std::string> fs_strings(fs.size());
        for (std::size_t i = 0; i < fs.size(); ++i) {
            fs_strings[i] = GetInputSymbol(SPrint(fs[i]));
        }
        ss << Join(m_word_separator, fs_strings);
    }
 
    ss << m_language_separator;
 
    if (!m_constrained_output_vocab) // unconstrained: faster version
        ss << Join(m_word_separator, es);
    else {  // here we need to check the pieces one by one
        std::vector<std::string> es_strings(fs.size());
        for (std::size_t i = 0; i < es.size(); ++i) {
            es_strings[i] = GetOutputSymbol(SPrint(es[i]));
        }
        ss << Join(m_word_separator, es_strings);
    }
    return ss.str();
}


void SparsePhrasePairMorphology::EvaluateWithSourceContext(const InputType &input
                             , const InputPath &inputPath
                             , const TargetPhrase &targetPhrase
                             , const StackVec *stackVec
                             , ScoreComponentCollection &scoreBreakdown
                             , ScoreComponentCollection *estimatedFutureScore) const
{
    std::vector< std::vector<StringPiece> > src_pieces;
    std::vector< std::vector<StringPiece> > tgt_pieces;
    GetPieces(inputPath.GetPhrase(), 
                m_input_factor,
                m_input_mode, 
                m_input_min_chars, 
                m_input_max_chars, 
                m_input_context, 
                m_input_reverse, 
                StringPiece(m_input_placeholder),
                src_pieces);
    GetPieces(targetPhrase, 
                m_output_factor,
                m_output_mode, 
                m_output_min_chars, 
                m_output_max_chars, 
                m_output_context, 
                m_output_reverse, 
                StringPiece(m_output_placeholder),
                tgt_pieces);
    for (std::size_t i = 0; i < src_pieces.size(); ++i) {
        for (std::size_t j = 0; j < tgt_pieces.size(); ++j) {
            FireFeature(scoreBreakdown, GetFeature(src_pieces[i], tgt_pieces[j]));
        }
    }
}


}




