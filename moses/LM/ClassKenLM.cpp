#include "moses/LM/ClassKenLM.h"

#include <vector>
#include <fstream>

#include "lm/binary_format.hh"

#include "moses/TypeDef.h"
#include "moses/Util.h"

using namespace std;

namespace Moses
{
namespace
{

void LoadWordToClassMapping(const std::string& path, std::map<std::string, std::string>& mapping, const bool lowercase_keys)
{
    mapping.clear();
    std::ifstream fin(path.c_str());
    std::string line;
    while (std::getline(fin, line)) {
        std::vector<std::string> tokens = Tokenize(line);
        if (tokens.empty()) 
            continue;
        UTIL_THROW_IF2(tokens.size() != 2, "Expected exactly 2 tokens: <word> <class>");
        if (lowercase_keys)
            mapping[ToLower(tokens[0])] = tokens[1];
        else
            mapping[tokens[0]] = tokens[1];
    }
}


} // namespace

template <class Model> ClassKenLM<Model>::ClassKenLM(const std::string &line, 
        const std::string &file, 
        FactorType factorType, 
        bool lazy, 
        const std::string& classPath,
        const bool lowercase_keys)
  :LanguageModelKen<Model>(line, file, factorType, lazy),
    m_lowercase_keys(lowercase_keys)
{
    LoadWordToClassMapping(classPath, m_word_to_class, lowercase_keys);
}

//template <class Model> ClassKenLM<Model>::ClassKenLM(const ClassKenLM<Model> &copy_from)
//    :LanguageModelKen<Model>(copy_from),
//    m_word_to_class(copy_from.m_word_to_class)
//{
//}

LanguageModel* ConstructClassKenLM(const std::string& line, 
        const std::string& file, 
        FactorType factorType, 
        bool lazy, 
        const std::string& classPath,
        const bool lowercase_keys)
{
  lm::ngram::ModelType model_type;
  if (lm::ngram::RecognizeBinary(file.c_str(), model_type)) {

    switch(model_type) {
    case lm::ngram::PROBING:
      return new ClassKenLM<lm::ngram::ProbingModel>(line, file, factorType, lazy, classPath, lowercase_keys);
    case lm::ngram::REST_PROBING:
      return new ClassKenLM<lm::ngram::RestProbingModel>(line, file, factorType, lazy, classPath, lowercase_keys);
    case lm::ngram::TRIE:
      return new ClassKenLM<lm::ngram::TrieModel>(line, file, factorType, lazy, classPath, lowercase_keys);
    case lm::ngram::QUANT_TRIE:
      return new ClassKenLM<lm::ngram::QuantTrieModel>(line, file, factorType, lazy, classPath, lowercase_keys);
    case lm::ngram::ARRAY_TRIE:
      return new ClassKenLM<lm::ngram::ArrayTrieModel>(line, file, factorType, lazy, classPath, lowercase_keys);
    case lm::ngram::QUANT_ARRAY_TRIE:
      return new ClassKenLM<lm::ngram::QuantArrayTrieModel>(line, file, factorType, lazy, classPath, lowercase_keys);
    default:
      UTIL_THROW2("Unrecognized kenlm model type " << model_type);
    }
  } else {
    return new ClassKenLM<lm::ngram::ProbingModel>(line, file, factorType, lazy, classPath, lowercase_keys);
  }
}

LanguageModel* ConstructClassKenLM(const std::string& line)
{
  FactorType factorType = 0;
  std::string filePath;
  std::string classPath;
  bool lazy = false;
  bool lowercase_keys = false;

  std::vector<std::string> toks = Tokenize(line);
  for (std::size_t i = 1; i < toks.size(); ++i) {
    std::vector<std::string> args = Tokenize(toks[i], "=");

    if (args[0] == "factor") {
      factorType = Scan<FactorType>(args[1]);
    } else if (args[0] == "order") {
      //nGramOrder = Scan<size_t>(args[1]);
    } else if (args[0] == "path") {
      UTIL_THROW_IF2(!FileExists(args[1]), "Model not found: " + args[1]);
      filePath = args[1];
    } else if (args[0] == "lazyken") {
      lazy = Scan<bool>(args[1]);
    } else if (args[0] == "mapping") {
      UTIL_THROW_IF2(!FileExists(args[1]), "Word-class mapping not found: " + args[1]);
      classPath = args[1];
    } else if (args[0] == "lowercase-keys") {
      if (args[1] == "true" || args[1] == "1" || args[1] == "yes")
        lowercase_keys = true;
    } else if (args[0] == "name") {
      // that's ok. do nothing, passes onto LM constructor
    }
  }

  UTIL_THROW_IF2(filePath.empty(), "ClassKenLM requires path=<path>");
  UTIL_THROW_IF2(classPath.empty(), "ClassKenLM requires word-to-class-mapping=<path>");

  return ConstructClassKenLM(line, filePath, factorType, lazy, classPath, lowercase_keys);
}


}

