#ifndef moses_SearchCubePruning_h
#define moses_SearchCubePruning_h

#include <vector>
#include "Search.h"
#include "HypothesisStackCubePruning.h"
#include "SentenceStats.h"

namespace Moses
{

class InputType;
class TranslationOptionCollection;

/** Functions and variables you need to decoder an input using the phrase-based decoder with cube-pruning
 *  Instantiated by the Manager class
 */
class SearchCubePruning: public Search
{
protected:
  const InputType &m_source;
  std::vector < HypothesisStack* > m_hypoStackColl; /**< stacks to store hypotheses (partial translations) */
  // no of elements = no of words in source + 1
  const TranslationOptionCollection &m_transOptColl; /**< pre-computed list of translation options for the phrases in this sentence */

  //! go thru all bitmaps in 1 stack & create backpointers to bitmaps in the stack
  void CreateForwardTodos(HypothesisStackCubePruning &stack);
  //! create a back pointer to this bitmap, with edge that has this words range translation
  void CreateForwardTodos(const WordsBitmap &bitmap, const WordsRange &range, BitmapContainer &bitmapContainer);
  bool CheckDistortion(const WordsBitmap &bitmap, const WordsRange &range) const;

  void PrintBitmapContainerGraph();
    
  inline bool WordLatticeCheckPathToStartPosition(const WordsBitmap &hypoBitmap, const std::size_t startPos) const
  {
    // first question: is there a path from the closest translated word to the left
    // of the hypothesized extension to the start of the hypothesized extension?
    // long version: is there anything to our left? is it farther left than where we're starting anyway? can we get to it?
    // closestLeft is exclusive: a value of 3 means 2 is covered, our arc is currently ENDING at 3 and can start at 3 implicitly
    std::size_t closestLeft = hypoBitmap.GetEdgeToTheLeftOf(startPos);
    return !(closestLeft != 0 && closestLeft != startPos && !m_source.CanIGetFromAToB(closestLeft, startPos));
  }

  inline bool WordLatticeCheckRange(const WordsRange& extRange) const
  {
    return m_source.IsCoveragePossible(extRange);
  }

  inline bool WordLatticeCheckPathFromEndPosition(const WordsBitmap &hypoBitmap, const std::size_t endPos) const
  {
    std::size_t closestRight = hypoBitmap.GetEdgeToTheRightOf(endPos);
    return !(closestRight != endPos && ((closestRight + 1) < m_source.GetSize()) && !m_source.CanIGetFromAToB(endPos + 1, closestRight + 1));
  }


public:
  SearchCubePruning(Manager& manager, const InputType &source, const TranslationOptionCollection &transOptColl);
  ~SearchCubePruning();

  void Decode();

  void OutputHypoStackSize();
  void OutputHypoStack(int stack);

  virtual const std::vector < HypothesisStack* >& GetHypothesisStacks() const;
  virtual const Hypothesis *GetBestHypothesis() const;
};


}
#endif
