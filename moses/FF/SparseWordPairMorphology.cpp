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
        ss << GetInputSymbol(SparseMorphology::StringPieceToString(f));
    else // unconstrained
        ss << f;
    
    ss << m_language_separator;
    
    if (m_constrained_output_vocab) // check the word
        ss << GetOutputSymbol(SparseMorphology::StringPieceToString(e));
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
    std::vector<StringPiece> src_pieces = GetPieces(inputPath.GetPhrase(), m_input_factor, m_input_max_chars, m_input_reverse);
    std::vector<StringPiece> tgt_pieces = GetPieces(targetPhrase, m_output_factor, m_output_max_chars, m_output_reverse);
    const Phrase& fphrase = inputPath.GetPhrase();
    const AlignmentInfo& ainfo = targetPhrase.GetAlignTerm();
    for (std::size_t f = 0; f < src_pieces.size(); ++f) {
        // retrieve word alignments
        std::set<std::size_t> es = ainfo.GetAlignmentsForSource(f);
        if (es.empty() && !m_fire_unaligned)  // we ignore unaligned words
            continue;
        // get a representation for the source word
        StringPiece frepr = SparseMorphology::GetPiece(fphrase.GetWord(f), m_input_factor, m_input_max_chars, m_input_reverse);
        // pair that representation with a representation for each target word f aligns to
        if (es.empty()) { // pair it with <none>
            FireFeature(scoreBreakdown, GetFeature(frepr, StringPiece(m_unaligned_repr)));
        } else {
            for (std::set<std::size_t>::const_iterator e = es.begin(); e != es.end(); ++e) {
                // get a representation for the target word
                StringPiece erepr = SparseMorphology::GetPiece(targetPhrase.GetWord(*e), m_output_factor, m_output_max_chars, m_output_reverse);
                FireFeature(scoreBreakdown, GetFeature(frepr, erepr));
            }
        }
    }
}

}





