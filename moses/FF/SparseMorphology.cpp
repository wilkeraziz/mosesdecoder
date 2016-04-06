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
    m_input_min_chars(0),
    m_input_max_chars(0),
    m_output_min_chars(0),
    m_output_max_chars(0),
    m_input_context(0),
    m_output_context(0),
    m_input_mode("token"),
    m_output_mode("token"),
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
    } else if (key == "input-min") {
        m_input_min_chars = Scan<std::size_t>(value);
    } else if (key == "input-max") {
        m_input_max_chars = Scan<std::size_t>(value);
    } else if (key == "input-context") {
        m_input_context = Scan<std::size_t>(value);
    } else if (key == "input-mode") {
        UTIL_THROW_IF2(value != "token" && value != "prefix" && value != "suffix", "Unknown option for input-mode=token|prefix|suffix: " + value);
        m_input_mode = Scan<std::string>(value);
    } else if (key == "input-short-token-placeholder") {
        m_input_placeholder = value;
    } else if (key == "output-short-token-placeholder") {
        m_output_placeholder = value;
    } else if (key == "output-min") {
        m_output_min_chars = Scan<std::size_t>(value);
    } else if (key == "output-max") {
        m_output_max_chars = Scan<std::size_t>(value);
    } else if (key == "output-context") {
        m_output_context = Scan<std::size_t>(value);
    } else if (key == "output-mode") {
        UTIL_THROW_IF2(value != "token" && value != "prefix" && value != "suffix", "Unknown option for output-mode=token|prefix|suffix: " + value);
        m_output_mode = Scan<std::string>(value);
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

    m_input_reverse = m_input_mode == "suffix";
    m_output_reverse = m_output_mode == "suffix";
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

 
StringPiece SparseMorphology::GetPiece(const Word& word, const FactorType factor, 
        const std::size_t size, 
        const std::size_t context,
        const bool reverse,
        const StringPiece& placeholder)
{
    const StringPiece piece = word.GetString(factor);
    if (size == 0) { // whole word
        if (piece.size() >= context)  
            return piece;
        if (placeholder.empty())  // the word doesnt meet the minimum requirements, but we were given an empty placeholder
            return piece;
        return placeholder;  
    } 
    if (piece.size() >= size + context) {
        if (reverse) { // suffix
            return piece.substr(piece.size() - size, size);
        } else {  // prefix
            return piece.substr(0, size);
        }
    } else { // not enough chars
        if (placeholder.empty())  // with an empty placeholder, we return the token
            return piece;
        return placeholder;
    }
        
}

std::vector<StringPiece> SparseMorphology::GetPieces(const Phrase& phrase, 
        const FactorType factor, 
        const std::size_t size, 
        const std::size_t context, 
        const bool reverse, 
        const StringPiece& placeholder)
{
    std::vector<StringPiece> pieces;
    pieces.reserve(phrase.GetSize());
    for (std::size_t i = 0; i < phrase.GetSize(); ++i) {
        pieces.push_back(SparseMorphology::GetPiece(phrase.GetWord(i), factor, size, context, reverse, placeholder));
    }
    return pieces;
}

void SparseMorphology::GetPieces(const Phrase& phrase, const FactorType& factor, const std::string& mode,
        const std::size_t min, const std::size_t max, const std::size_t context,
        const bool reverse,
        const StringPiece& placeholder,
        std::vector< std::vector<StringPiece> >& pieces)
{
    pieces.clear();
    if (mode == "token") { 
        pieces.push_back(GetPieces(phrase, 
                    factor, 
                    0, // that's how we select all chars
                    context, // we still require a minimum length
                    reverse, 
                    placeholder));
    }
    else {
        for (std::size_t size = min; size <= max; ++size) {
            pieces.push_back(GetPieces(phrase, 
                        factor, 
                        size, 
                        context, 
                        reverse, 
                        placeholder));
        }
    }
}

}


