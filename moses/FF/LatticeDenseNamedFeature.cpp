#include "moses/FF/LatticeDenseNamedFeature.h"
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <set>
#include <fstream>
#include <iostream>
#include "moses/WordsBitmap.h"
#include "moses/WordsRange.h"
#include "moses/Hypothesis.h"
#include "moses/ScoreComponentCollection.h"
#include "moses/TargetPhrase.h"
#include "moses/InputType.h"
#include "moses/Util.h"
#include "moses/AlignmentInfo.h"
#include "moses/InputPath.h"

using namespace std;

namespace Moses
{

 LatticeDenseNamedFeature::LatticeDenseNamedFeature(const std::string &line)
  :StatelessFeatureFunction(1, line)
{
    ReadParameters();
    // sanity checks
    UTIL_THROW_IF2(m_fname.empty(),
          "LatticeDenseNamedFeature requires a key (key=<name>).");

}

/**
 * CONFIG FF
 */

void LatticeDenseNamedFeature::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "key") {
        m_fname = value;
    } else {
        StatelessFeatureFunction::SetParameter(key, value);
    }
}

/**
 * COMPUTE FF
 */
  
void LatticeDenseNamedFeature::EvaluateInIsolation(const InputPath &inputPath
                       , const TargetPhrase &targetPhrase
                       , ScoreComponentCollection &scoreBreakdown
                       , ScoreComponentCollection &estimatedFutureScore) const
{
    float value = 0.0;
    const ScorePair* s = inputPath.GetInputScore(); // << std::endl; 
    if (s != NULL) {
        std::map<StringPiece, float>::const_iterator it = s->sparseScores.find(m_fname);
        if (it != s->sparseScores.end())
            value = it->second;
    }
    scoreBreakdown.PlusEquals(this, value);
}

void LatticeDenseNamedFeature::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{
    float value = 0.0;
    const ScorePair* s = inputPath.GetInputScore(); // << std::endl; 
    if (s != NULL) {
        std::map<StringPiece, float>::const_iterator it = s->sparseScores.find(m_fname);
        if (it != s->sparseScores.end())
            value = it->second;
    }
    scoreBreakdown.PlusEquals(this, value);
}

void LatticeDenseNamedFeature::EvaluateWhenApplied(const Hypothesis& hypo,
    ScoreComponentCollection* accumulator) const
{
}

void LatticeDenseNamedFeature::EvaluateWhenApplied(const ChartHypothesis &hypo,
    ScoreComponentCollection* accumulator) const
{
}


}


