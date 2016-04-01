#include "moses/FF/SparseMorphology.h"
#include "moses/Util.h"

using namespace std;

namespace Moses
{

 SparseMorphology::SparseMorphology(const std::string &line)
  :StatelessFeatureFunction(0, line)
{
    ReadParameters();

}

/**
 * CONFIG FF
 */

void SparseMorphology::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "input-factor") {
        m_input_factor = Scan<FactorType>(value);
    } else if (key == "output-factor") {
        m_output_factor = Scan<FactorType>(value);
    } else if (key == "input-max-chars") {
        m_input_max_chars = Scan<int>(value);
    } else if (key == "output-max-chars") {
        m_output_max_chars = Scan<int>(value);
    } else {
        StatelessFeatureFunction::SetParameter(key, value);
    }
}

/**
 * HELPER
 */
 
std::vector<StringPiece> SparseMorphology::GetPieces(const Phrase& phrase, const FactorType factor, const int size)
{
    std::vector<StringPiece> pieces(phrase.GetSize());
    for (std::size_t i = 0; i < phrase.GetSize(); ++i) {
        const StringPiece& w = phrase.GetWord(i).GetString(factor);
        if (size == 0) // whole word
            pieces[i] = w;
        else if (size > 0) { // suffix
            const std::size_t start = ((int)w.size() > size)? (int)w.size() - size : 0;
            pieces[i] = w.substr(start, size);
        } else {  // prefix
            pieces[i] = w.substr(0, -size);
        }
    }
    return pieces;
}


}


