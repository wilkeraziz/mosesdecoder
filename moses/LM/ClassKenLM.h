#pragma once

#include <string>
#include <map>

#include "util/string_piece.hh"
#include <boost/shared_ptr.hpp>

#include "lm/word_index.hh"
#include "lm/model.hh"

#include "moses/LM/Base.h"
#include "moses/Hypothesis.h"
#include "moses/TypeDef.h"
#include "moses/Word.h"
#include "moses/Util.h"
#include "moses/LM/Ken.h"

namespace Moses
{

LanguageModel* ConstructClassKenLM(const std::string& line);

LanguageModel* ConstructClassKenLM(const std::string& line, 
        const std::string& file, 
        FactorType factorType, 
        bool lazy, 
        const std::string& classPath,
        const bool lowercase_keys);


template <class Model> class ClassKenLM : public LanguageModelKen<Model>
{
public:
  ClassKenLM(const std::string &line, 
          const std::string &file, 
          FactorType factorType, 
          bool lazy, 
          const std::string& classPath,
          const bool lowercase_keys);

protected:
  std::map<std::string, std::string> m_word_to_class;
  bool m_lowercase_keys;

  lm::WordIndex TranslateID(const Word &word) const {
      std::map<std::string, std::string>::const_iterator it;
      if (!m_lowercase_keys)
         it = m_word_to_class.find(SPrint(word.GetString(this->m_factorType)));
      else
         it = m_word_to_class.find(ToLower(SPrint(word.GetString(this->m_factorType))));
      if (it == m_word_to_class.end())
        return this->m_ngram->GetVocabulary().Index(word.GetString(this->m_factorType)); 
      return this->m_ngram->GetVocabulary().Index(it->second); 
  }

  //ClassKenLM(const ClassKenLM<Model> &copy_from);

private:


};


} // namespace Moses

