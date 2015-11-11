/*
 * Distortion.h
 *
 *  Created on: 28 Oct 2015
 *      Author: hieu
 */

#ifndef DISTORTION_H_
#define DISTORTION_H_

#include "StatefulFeatureFunction.h"
#include "../legacy/Range.h"
#include "../TypeDef.h"

class Distortion : public StatefulFeatureFunction
{
public:
	Distortion(size_t startInd, const std::string &line);
	virtual ~Distortion();

  virtual Moses::FFState* BlankState(const Manager &mgr, const PhraseImpl &input) const;
  virtual void EmptyHypothesisState(Moses::FFState &state, const Manager &mgr, const PhraseImpl &input) const;

  virtual void
  EvaluateInIsolation(const System &system,
		  const Phrase &source,
		  const TargetPhrase &targetPhrase,
		  Scores &scores,
		  Scores *estimatedScores) const;

  virtual void EvaluateWhenApplied(const Manager &mgr,
    const Hypothesis &hypo,
    const Moses::FFState &prevState,
    Scores &scores,
	Moses::FFState &state) const;

protected:
  SCORE CalculateDistortionScore(const Range &prev, const Range &curr, const int FirstGap) const;

  int ComputeDistortionDistance(const Range& prev, const Range& current) const;

};

#endif /* DISTORTION_H_ */
