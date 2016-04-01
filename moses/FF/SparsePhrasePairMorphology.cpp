#include "moses/FF/SparsePhrasePairMorphology.h"
#include "moses/Util.h"

using namespace std;

namespace Moses
{

 SparsePhrasePairMorphology::SparsePhrasePairMorphology(const std::string &line)
  :SparseMorphology(line)
{
    ReadParameters();
}

/**
 * CONFIG FF
 */

/**
 * HELPER
 */

/*
 
void SparsePhrasePairMorphology::EvaluateWithSourceContext(const InputType &input
                             , const InputPath &inputPath
                             , const TargetPhrase &targetPhrase
                             , const StackVec *stackVec
                             , ScoreComponentCollection &scoreBreakdown
                             , ScoreComponentCollection *estimatedFutureScore = NULL) const
{
    std::vector<StringPiece> src_morph = SparseMorphology::GetPieces(inputPath.GetPhrase(), m_input_factor, m_input_max_chars);
    std::vector<StringPiece> tgt_morph = SparseMorphology::GetPieces(targetPhrase, m_output_factor, m_output_max_chars);
    
}
*/

}




