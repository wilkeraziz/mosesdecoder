/*
 * Word.h
 *
 *  Created on: 23 Oct 2015
 *      Author: hieu
 */

#pragma once

#include <iostream>
#include "TypeDef.h"
#include "legacy/Factor.h"
#include "legacy/FactorCollection.h"

namespace Moses2
{

class Word {
	  friend std::ostream& operator<<(std::ostream &, const Word &);
public:
  Word();
  Word(const Word &copy);

  virtual ~Word();

  void CreateFromString(FactorCollection &vocab, const System &system, const std::string &str);

  size_t hash() const;
  int Compare(const Word &compare) const;

  bool operator==(const Word &compare) const
  {
	int cmp = Compare(compare);
	return cmp == 0;
  }

  bool operator<(const Word &compare) const;

  const Factor* operator[](size_t ind) const {
	return m_factors[ind];
  }

  const Factor*& operator[](size_t ind) {
	return m_factors[ind];
  }

protected:
  const Factor *m_factors[MAX_NUM_FACTORS];

};

}
