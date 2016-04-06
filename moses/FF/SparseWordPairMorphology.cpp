#include "moses/FF/SparseWordPairMorphology.h"
#include "moses/Util.h"
#include <sstream>

using namespace std;

namespace Moses
{

 SparseWordPairMorphology::SparseWordPairMorphology(const std::string &line)
  :SparseMorphology(line), 
    m_fire_unaligned(false), 
    m_unaligned_repr("<null>")
{
    ReadParameters();
}

void SparseWordPairMorphology::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "fire-unaligned") {
        if (value == "true" || value == "1" || value == "yes")
            m_fire_unaligned = true;
    } else if (key == "unaligned-repr") {
        m_unaligned_repr = value;
    } else {
        SparseMorphology::SetParameter(key, value);
    }
}

std::string SparseWordPairMorphology::GetFeature(const StringPiece &f, const StringPiece &e) const
{
    std::stringstream ss;
    if (m_constrained_input_vocab)  // check the word
        ss << GetInputSymbol(SPrint(f));
    else // unconstrained
        ss << f;
    
    ss << m_language_separator;
    
    if (m_constrained_output_vocab) // check the word
        ss << GetOutputSymbol(SPrint(e));
    else // unconstrained
        ss << e;
    return ss.str();
}



void SparseWordPairMorphology::EvaluateWithSourceContext(const InputType &input
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
    
    const AlignmentInfo& ainfo = targetPhrase.GetAlignTerm();
    for (std::size_t i = 0; i < src_pieces.size(); ++i) {
        for (std::size_t j = 0; j < tgt_pieces.size(); ++j) {
            for (std::size_t f = 0; f < src_pieces[i].size(); ++f) {
                // retrieve word alignments
                std::set<std::size_t> es = ainfo.GetAlignmentsForSource(f);
                if (es.empty() && !m_fire_unaligned)  // we ignore unaligned words
                    continue;
                // get a representation for the source word
                /*StringPiece frepr = SparseMorphology::GetPiece(fphrase.GetWord(f), 
                        m_input_factor, 
                        m_input_max_chars, 
                        m_input_context, 
                        m_input_reverse, 
                        StringPiece(m_input_placeholder));*/
                // pair that representation with a representation for each target word f aligns to
                if (es.empty()) { // pair it with <none>
                    FireFeature(scoreBreakdown, GetFeature(src_pieces[i][f], StringPiece(m_unaligned_repr)));
                } else {
                    for (std::set<std::size_t>::const_iterator e = es.begin(); e != es.end(); ++e) {
                        // get a representation for the target word
                        /*StringPiece erepr = SparseMorphology::GetPiece(targetPhrase.GetWord(*e), 
                                m_output_factor, 
                                m_output_max_chars, 
                                m_output_context, 
                                m_output_reverse, 
                                StringPiece(m_output_placeholder));*/
                        FireFeature(scoreBreakdown, GetFeature(src_pieces[i][f], tgt_pieces[j][*e]));
                    }
                }
            }


        }
    }

}

}





