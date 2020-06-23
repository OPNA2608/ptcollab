#ifndef PXTONEEDITACTION_H
#define PXTONEEDITACTION_H

#include <QDataStream>
#include <QDebug>
#include <QList>
#include <optional>
#include <set>
#include <vector>

#include "pxtone/pxtnService.h"
// Because unit nos are fixed in pxtone, we have to maintain this mapping so
// that actions are still valid past unit additions / deletions / moves.
class UnitIdMap {
 public:
  UnitIdMap(const pxtnService *pxtn) : m_next_id(0) {
    for (int32_t i = 0; i < pxtn->Unit_Num(); ++i) addUnit();
  }
  qint32 noToId(qint32 no) const { return m_no_to_id[no]; };
  std::optional<qint32> idToNo(qint32 id) const {
    auto it = m_id_to_no.find(id);
    if (it == m_id_to_no.end()) return std::nullopt;
    return qint32(it->second);
  };
  void addUnit() {
    qint32 id = m_next_id++;
    size_t no = m_no_to_id.size();

    // equiv: m_no_to_id[no] = id;
    m_no_to_id.push_back(id);
    m_id_to_no[id] = no;

    // qDebug() << QVector<qint32>(m_no_to_id.begin(), m_no_to_id.end())
    //         << QMap<qint32, qint32>(m_id_to_no);
  }
  void removeUnit(size_t no) {
    qint32 id = m_no_to_id[no];
    m_no_to_id.erase(m_no_to_id.begin() + no);
    m_id_to_no.erase(id);
    for (auto &it : m_id_to_no)
      if (it.second > no) --it.second;

    // qDebug() << QVector<qint32>(m_no_to_id.begin(), m_no_to_id.end())
    //         << QMap<qint32, size_t>(m_id_to_no);
  }
  // TODO: move unit

 private:
  int m_next_id;
  std::map<qint32, size_t> m_id_to_no;
  std::vector<qint32> m_no_to_id;
};

template <typename T>
QDataStream &read_as_qint8(QDataStream &in, T &x) {
  qint8 v;
  in >> v;
  x = T(v);
  return in;
}

struct Action {
  enum Type { ADD, DELETE };  // Add implicitly means add to an empty space
  Type type;
  EVENTKIND kind;
  qint32 unit_id;
  qint32 start_clock;
  qint32 end_clock_or_value;  // end clock if delete, value if add
  void perform(pxtnService *pxtn, bool *widthChanged,
               const UnitIdMap &map) const;
  std::list<Action> get_undo(const pxtnService *pxtn,
                             const UnitIdMap &map) const;
  void print() const;
};
QDataStream &operator<<(QDataStream &out, const Action &a);
QDataStream &operator>>(QDataStream &in, Action &a);

// You have to compute the undo at the time the original action was applied in
// the case of collaborative editing. You can't compute it beforehand.
//
// I guess what I really want is a
// std::vector<Action> pxtnEvelist::apply(const std::vector<Action> &actions)
// That gives me the undo result.

std::list<Action> apply_actions_and_get_undo(const std::list<Action> &actions,
                                             pxtnService *pxtn,
                                             bool *widthChanged,
                                             const UnitIdMap &map);

#endif  // PXTONEEDITACTION_H
