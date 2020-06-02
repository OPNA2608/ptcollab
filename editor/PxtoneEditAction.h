#ifndef PXTONEEDITACTION_H
#define PXTONEEDITACTION_H

#include <set>
#include <vector>

#include "pxtone/pxtnEvelist.h"
struct Action {
  enum Type { ADD, DELETE };  // Add implicitly means add to an empty space
  Type type;
  EVENTKIND kind;
  int unit_no;
  int start_clock;
  int end_clock_or_value;  // depending on type
  void perform(pxtnEvelist *evels) const;
  void print() const;
};

// You have to compute the undo at the time the original action was applied in
// the case of collaborative editing. You can't compute it beforehand.
//
// I guess what I really want is a
// std::vector<Action> pxtnEvelist::apply(const std::vector<Action> &actions)
// That gives me the undo result.

std::vector<Action> apply_actions_and_get_undo(
    const std::vector<Action> &actions, pxtnEvelist *evels);

#endif  // PXTONEEDITACTION_H
