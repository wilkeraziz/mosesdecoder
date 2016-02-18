#include <cstdlib>
#include <algorithm>
#include <vector>
#include <set>
#include <fstream>
#include <iostream>
#include "moses/WordsBitmap.h"
#include "moses/WordsRange.h"
#include "moses/Hypothesis.h"
#include "LatticeKTauSimple.h"
#include "moses/ScoreComponentCollection.h"
#include "moses/TargetPhrase.h"
#include "moses/InputType.h"
#include "moses/Util.h"
#include "moses/AlignmentInfo.h"
#include "moses/InputPath.h"

using namespace std;

namespace Moses
{

 LatticeKTauSimple::LatticeKTauSimple(const std::string &line)
  :StatelessFeatureFunction(1, line), m_fname("KTau"), m_isolation(true)
{
    ReadParameters();
}

/**
 * CONFIG FF
 */

void LatticeKTauSimple::SetParameter(const std::string& key, const std::string& value)
{
    if (key == "fname") {
        m_fname = value;
    } else if (key == "isolation") {
        if (value == "1" || value == "true" || value == "TRUE" || value == "True") {
            m_isolation = true;
        } else {
            m_isolation = false;
        }
    } else {
        StatelessFeatureFunction::SetParameter(key, value);
    }
}

/**
 * COMPUTE FF
 */
  
void LatticeKTauSimple::InitializeForInput(InputType const& source) 
{
    m_seg = source.GetTranslationId();  // the sequential id of the segment being translated
}
  
void LatticeKTauSimple::EvaluateInIsolation(const InputPath &inputPath
                       , const TargetPhrase &targetPhrase
                       , ScoreComponentCollection &scoreBreakdown
                       , ScoreComponentCollection &estimatedFutureScore) const
{
    //std::clog << "EvaluateInIsolation: range=" << inputPath.GetWordsRange() << " source=" << inputPath.GetPhrase() << " target=" << targetPhrase << std::endl;
    if (m_isolation) {
        float expectation = 0.0;
        const ScorePair* s = inputPath.GetInputScore(); // << std::endl; 
        if (s != NULL) {
            std::map<StringPiece, float>::const_iterator it = s->sparseScores.find(m_fname);
            if (it != s->sparseScores.end())
                expectation = it->second;
        }
        std::vector<float> scores(GetNumScoreComponents(), 0.0);
        SetKTau(scores, expectation);
        scoreBreakdown.PlusEquals(this, scores);
    }
}

void LatticeKTauSimple::EvaluateWithSourceContext(const InputType &input
    , const InputPath &inputPath
    , const TargetPhrase &targetPhrase
    , const StackVec *stackVec
    , ScoreComponentCollection &scoreBreakdown
    , ScoreComponentCollection *estimatedFutureScore) const
{
    float expectation = 0.0;
    const ScorePair* s = inputPath.GetInputScore(); // << std::endl; 
    if (s != NULL) {
        //std::clog << "dense=";
        //for (std::vector<float>::const_iterator it = s->denseScores.begin(); it != s->denseScores.end(); ++it)
        //    std::clog << *it << " ";
        //std::clog << std::endl;
        //std::clog << "sparse=";
        //for (std::map<StringPiece,float>::const_iterator it = s->sparseScores.begin(); it != s->sparseScores.end(); ++it)
        //    std::clog << it->first << "=" << it->second << " ";
        //std::clog << std::endl;
        std::map<StringPiece, float>::const_iterator it = s->sparseScores.find(m_fname);
        if (it != s->sparseScores.end())
            expectation = it->second;
    }
    std::vector<float> scores(GetNumScoreComponents(), 0.0);
    SetKTau(scores, expectation);
    scoreBreakdown.PlusEquals(this, scores);
}

void LatticeKTauSimple::EvaluateWhenApplied(const Hypothesis& hypo,
    ScoreComponentCollection* accumulator) const
{
    //const WordsBitmap& bmap = hypo.GetWordsBitmap();
    //const WordsRange& curr = hypo.GetCurrSourceWordsRange();
}

void LatticeKTauSimple::EvaluateWhenApplied(const ChartHypothesis &hypo,
    ScoreComponentCollection* accumulator) const
{
}




}

