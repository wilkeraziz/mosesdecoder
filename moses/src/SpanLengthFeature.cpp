#include "SpanLengthFeature.h"
#include "util/check.hh"
#include "FFState.h"
#include "TranslationOption.h"
#include "WordsRange.h"
#include "ChartHypothesis.h"
#include "DynSAInclude/types.h"

namespace Moses {
  
SpanLengthFeature::SpanLengthFeature(ScoreIndexManager &scoreIndexManager, const std::vector<float> &weight)
  : m_withTargetLength(weight.size() > 1)
{
  scoreIndexManager.AddScoreProducer(this);
  const_cast<StaticData&>(StaticData::Instance()).SetWeightsForScoreProducer(this, weight);
}
  
size_t SpanLengthFeature::GetNumScoreComponents() const
{
  return m_withTargetLength ? 2 : 1;
}
  
std::string SpanLengthFeature::GetScoreProducerDescription(unsigned id) const
{
  CHECK(id < 2);
  if (id == 0)
    return "SpanLengthSource";
  else
    return "SpanLengthTarget";
}

std::string SpanLengthFeature::GetScoreProducerWeightShortName(unsigned id) const
{
  if (id == 0)
    return "SLS";
  else
    return "SLT";
}

size_t SpanLengthFeature::GetNumInputScores() const {
  return 0;
}
  
const FFState* SpanLengthFeature::EmptyHypothesisState(const InputType &input) const
{
  return NULL; // not saving any kind of state so far
}

FFState* SpanLengthFeature::Evaluate(
  const Hypothesis &/*cur_hypo*/,
  const FFState */*prev_state*/,
  ScoreComponentCollection */*accumulator*/) const
{
  return NULL;
}
  
class SpanLengthFeatureState : public FFState
{
public:
  SpanLengthFeatureState(unsigned terminalCount)
  : m_terminalCount(terminalCount)
  {
  }
  
  unsigned GetTerminalCount() const {
    return m_terminalCount;
  }
  
  virtual int Compare(const FFState& other) const {
    int otherTerminalCount = (dynamic_cast<const SpanLengthFeatureState&>(other)).m_terminalCount;
    if (m_terminalCount < otherTerminalCount)
      return -1;
    if (m_terminalCount > otherTerminalCount)
      return 1;
    return 0;
  }
private:
  unsigned m_terminalCount;
};
  
struct SpanInfo {
  unsigned SourceSpan, TargetSpan;
  unsigned SourceRangeStart;
};
  
static bool operator<(const SpanInfo& left, const SpanInfo& right)
{
  return left.SourceRangeStart < right.SourceRangeStart;
}
  
static std::vector<SpanInfo> GetSpans(const ChartHypothesis &chartHypothesis, int featureId)
{
  const std::vector<const ChartHypothesis*>& prevHypos = chartHypothesis.GetPrevHypos();
  std::vector<SpanInfo> result;
  result.reserve(prevHypos.size());
  iterate(prevHypos, iter) {
    WordsRange curRange = (*iter)->GetCurrSourceRange();
    SpanInfo spanInfo = {
      curRange.GetNumWordsCovered(),
      unsigned(-1),
      curRange.GetStartPos()
    };
    const SpanLengthFeatureState* state = dynamic_cast<const SpanLengthFeatureState*>((*iter)->GetFFState(featureId));
    if (state != NULL) {
      spanInfo.TargetSpan = state->GetTerminalCount();
    }
    result.push_back(spanInfo);
  }
  std::sort(result.begin(), result.end());
  return result;
}

FFState* SpanLengthFeature::EvaluateChart(
  const ChartHypothesis &chartHypothesis,
  int featureId,
  ScoreComponentCollection *accumulator) const
{
  std::vector<SpanInfo> spans = GetSpans(chartHypothesis, featureId);
  const TargetPhrase& targetPhrase = chartHypothesis.GetCurrTargetPhrase();
  std::vector<float> scores(GetNumScoreComponents());
  for (size_t spanIndex = 0; spanIndex < spans.size(); ++spanIndex) {
    float score = targetPhrase.GetScoreBySourceSpanLength(spanIndex, spans[spanIndex].SourceSpan);
    scores[0] += score;
  }
  if (m_withTargetLength) {
    unsigned terminalCount = (unsigned)chartHypothesis.GetCurrTargetPhrase().GetSize();
    for (size_t spanIndex = 0; spanIndex < spans.size(); ++spanIndex) {
      terminalCount += spans[spanIndex].TargetSpan;
      --terminalCount;
      float score = targetPhrase.GetScoreByTargetSpanLength(spanIndex,spans[spanIndex].TargetSpan);
      scores[1] += score;
    }
    accumulator->PlusEquals(this, scores);
    return new SpanLengthFeatureState(terminalCount);
  } else {
    accumulator->PlusEquals(this, scores);
    return NULL;
  }
}
  
} // namespace moses