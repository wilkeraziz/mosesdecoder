#pragma once

#include <vector>
#include <map>
#include <string>
#include "moses/FF/StatelessFeatureFunction.h"
#include "moses/InputPath.h"
#include "moses/Hypothesis.h"
#include "moses/ChartHypothesis.h"
#include "moses/TypeDef.h"
#include "util/string_piece.hh"
#include "moses/FF/SparseMorphology.h"

namespace Moses
{

class SparsePhrasePairMorphology : public SparseMorphology
{
    
public:
  SparsePhrasePairMorphology(const std::string &line);

  bool IsUseable(const FactorMask &mask) const {
    return true;
  }
  
  void EvaluateWithSourceContext(const InputType &input
                                 , const InputPath &inputPath
                                 , const TargetPhrase &targetPhrase
                                 , const StackVec *stackVec
                                 , ScoreComponentCollection &scoreBreakdown
                                 , ScoreComponentCollection *estimatedFutureScore = NULL) const
  {
  }

};

}




