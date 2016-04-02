#include "moses/FF/SparseMorphology.h"
#include "moses/Util.h"
#include <sstream>

using namespace std;

namespace Moses
{

 SparseMorphology::SparseMorphology(const std::string &line)
  :StatelessFeatureFunction(0, line),
    m_input_factor(0),
    m_output_factor(0),
    m_input_max_chars(0),
    m_output_max_chars(0),
    m_constrained_vocab(false),
    m_ignore_prefix(false),
    m_language_separator("=>"),
    m_constrained_input_vocab(false),
    m_constrained_output_vocab(false),
    m_input_oov("*"),
    m_output_oov("*")
{
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
        const int tmp = Scan<int>(value);
        if (tmp >= 0) {
            m_input_max_chars = (std::size_t)tmp;
            m_input_reverse = false;
        } else {
            m_input_max_chars = (std::size_t) -tmp;
            m_input_reverse = true;
        }
    } else if (key == "output-max-chars") {
        const int tmp = Scan<int>(value);
        if (tmp >= 0) {
            m_output_max_chars = (std::size_t)tmp;
            m_output_reverse = false;
        } else {
            m_output_max_chars = (std::size_t) -tmp;
            m_output_reverse = true;
        }
    } else if (key == "feature-vocab") {
        UTIL_THROW_IF2(!FileExists(value), "Vocabulary of features not found: " + value);
        m_vocab_path = value;
        m_constrained_vocab = true;
    } else if (key == "ignore-prefix-in-feature-vocab") {
        if (value == "true" || value == "1" || value == "yes")
            m_ignore_prefix = true;
    } else if (key == "language-separator") {
        m_language_separator = value;
    } else if (key == "input-vocab") {
        UTIL_THROW_IF2(!FileExists(value), "Input vocabulary file not found: " + value);
        m_input_vocab_path = value;
        m_constrained_input_vocab = true;
    } else if (key == "output-vocab") {
        UTIL_THROW_IF2(!FileExists(value), "Output vocabulary file not found: " + value);
        m_output_vocab_path = value;
        m_constrained_output_vocab = true;
    } else if (key == "oov-feature") {
        m_missing_feature = value;
    } else if (key == "input-oov") {
        m_input_oov = value;
    } else if (key == "output-oov") {
        m_output_oov = value;
    } else {
        StatelessFeatureFunction::SetParameter(key, value);
    }
}

void SparseMorphology::Load() 
{
    if (!m_vocab_path.empty())
        ReadVocab(m_vocab_path, m_features, m_ignore_prefix);
    if (!m_input_vocab_path.empty())
        ReadVocab(m_input_vocab_path, m_input_vocab, false);
    if (!m_output_vocab_path.empty())
        ReadVocab(m_output_vocab_path, m_output_vocab, false);
}

void SparseMorphology::ReadVocab(const std::string& path, std::set<std::string>& features, bool ignore_prefix)
{
    features.clear();
    std::ifstream fin(path.c_str());
    std::string line;
    std::size_t i = 1;
    while (std::getline(fin, line)) {
        line = Trim(line);
        std::vector<std::string> tokens = Tokenize(line);  // this is to get rid of weights (in case one is reusing a weight file as a means to filter features)
        if (tokens.empty())  // skip empty lines
            continue;
        UTIL_THROW_IF2(tokens.size() > 2, "Line " + SPrint(i) + ". Expected at most two tokens <feature-name> <weight>, got: " + line);
        std::string feature = tokens.front();
        if (ignore_prefix) {  // here we get rid of a component name prefixing the feature name (we do not check whether this component name matches this FF)
            std::vector<std::string> parts = TokenizeFirstOnly(feature, "_");
            UTIL_THROW_IF2(parts.size() < 2, "Line " + SPrint(i) + ". Expected <prefix>_<feature>, got: " + feature);
            feature = parts.back();
        }
        UTIL_THROW_IF2(feature.empty(), "Line " + SPrint(i) + ". Feature names cannot be empty: " + line);
        features.insert(feature);
        ++i;
    }
}

/**
 * HELPER
 */

bool SparseMorphology::FireFeature(ScoreComponentCollection& scores, const std::string& feature) const
{
    if (!m_constrained_vocab || m_features.find(feature) != m_features.end()) {
        std::stringstream ss;
        ss << GetScoreProducerDescription() << "_" << feature;
        scores.SparsePlusEquals(ss.str(), 1.0);
        return true;
    } else if (!m_missing_feature.empty()) {
        std::stringstream ss;
        ss << GetScoreProducerDescription() << "_" << m_missing_feature;
        scores.SparsePlusEquals(ss.str(), 1.0);
        return true;
    }
    return false;
}

 
StringPiece SparseMorphology::GetPiece(const Word& word, const FactorType factor, const std::size_t size, bool reverse)
{
    const StringPiece piece = word.GetString(factor);
    if (size == 0) // whole word
        return piece;
    else if (reverse) { // suffix
        const std::size_t start = (piece.size() > size)? piece.size() - size : 0;
        return piece.substr(start, size);
    } else {  // prefix
        return piece.substr(0, size);
    }
}

std::vector<StringPiece> SparseMorphology::GetPieces(const Phrase& phrase, const FactorType factor, const std::size_t size, bool reverse)
{
    std::vector<StringPiece> pieces(phrase.GetSize());
    for (std::size_t i = 0; i < phrase.GetSize(); ++i) {
        pieces[i] = SparseMorphology::GetPiece(phrase.GetWord(i), factor, size, reverse);
    }
    return pieces;
}

}


