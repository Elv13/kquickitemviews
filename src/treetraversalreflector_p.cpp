/***************************************************************************
 *   Copyright (C) 2017-2018 by Emmanuel Lepage Vallee                     *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@kde.org>             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#include "treetraversalreflector_p.h"

// KQuickItemViews
#include "adapters/abstractitemadapter_p.h"
#include "adapters/abstractitemadapter.h"
#include "visiblerange.h"
#include "visiblerange_p.h"

// Qt
#include <QQmlComponent>
#include <QtCore/QDebug>

// Use some constant for readability
#define PREVIOUS 0
#define NEXT 1
#define FIRST 0
#define LAST 1
/**
 * Hold the QPersistentModelIndex and the metadata associated with them.
 */
struct TreeTraversalItems
{
    explicit TreeTraversalItems(TreeTraversalItems* parent, TreeTraversalReflectorPrivate* d):
        m_pParent(parent), d_ptr(d) {}

    enum class State {
        BUFFER    = 0, /*!< Not in use by any visible indexes, but pre-loaded */
        REMOVED   = 1, /*!< Currently in a removal transaction                */
        REACHABLE = 2, /*!< The [grand]parent of visible indexes              */
        VISIBLE   = 3, /*!< The element is visible on screen                  */
        ERROR     = 4, /*!< Something went wrong                              */
        DANGLING  = 5, /*!< Being destroyed                                   */
    };

    enum class Action {
        SHOW   = 0, /*!< Make visible on screen (or buffer) */
        HIDE   = 1, /*!< Remove from the screen (or buffer) */
        ATTACH = 2, /*!< Track, but do not show             */
        DETACH = 3, /*!< Stop tracking for changes          */
        UPDATE = 4, /*!< Update the element                 */
        MOVE   = 5, /*!< Update the depth and lookup        */
        RESET  = 6, /*!< Flush the visual item              */
    };

    typedef bool(TreeTraversalItems::*StateF)();

    static const State  m_fStateMap    [6][7];
    static const StateF m_fStateMachine[6][7];

    bool performAction(Action);

    bool nothing();
    bool error  ();
    bool show   ();
    bool hide   ();
    bool attach ();
    bool detach ();
    bool refresh();
    bool index  ();
    bool destroy();
    bool reset  ();

    // Geometric navigation
    TreeTraversalItems* up   () const;
    TreeTraversalItems* down () const;
    TreeTraversalItems* left () const;
    TreeTraversalItems* right() const;

    // Helpers
    bool updateVisibility();

    //TODO use a btree, not an hash
    QHash<QPersistentModelIndex, TreeTraversalItems*> m_hLookup;

    uint m_Depth {0};
    State m_State {State::BUFFER};

    // Keep the parent to be able to get back to the root
    TreeTraversalItems* m_pParent;

    TreeTraversalItems* m_tSiblings[2] = {nullptr, nullptr};
    TreeTraversalItems* m_tChildren[2] = {nullptr, nullptr};

    // Because slotRowsMoved is called before the change take effect, cache
    // the "new real row and column" since the current index()->row() is now
    // garbage.
    int m_MoveToRow    {-1};
    int m_MoveToColumn {-1};

    QPersistentModelIndex m_Index;
    VisualTreeItem* m_pTreeItem {nullptr};

    //TODO keep the absolute index of the GC circular buffer

    TreeTraversalReflectorPrivate* d_ptr;
};

class TreeTraversalReflectorPrivate : public QObject
{
    Q_OBJECT
public:

    // Helpers
    TreeTraversalItems* addChildren(TreeTraversalItems* parent, const QModelIndex& index);
    void bridgeGap(TreeTraversalItems* first, TreeTraversalItems* second, bool insert = false);
    void createGap(TreeTraversalItems* first, TreeTraversalItems* last  );
    TreeTraversalItems* ttiForIndex(const QModelIndex& idx) const;

    void setTemporaryIndices(const QModelIndex &parent, int start, int end,
                             const QModelIndex &destination, int row);
    void resetTemporaryIndices(const QModelIndex &parent, int start, int end,
                               const QModelIndex &destination, int row);

    TreeTraversalItems* m_pRoot {new TreeTraversalItems(nullptr, this)};

    //TODO add a circular buffer to GC the items
    // * relative index: when to trigger GC
    // * absolute array index: kept in the TreeTraversalItems so it can
    //   remove itself

    /// All elements with loaded children
    QHash<QPersistentModelIndex, TreeTraversalItems*> m_hMapper;
    QAbstractItemModel* m_pModel {nullptr};
    std::function<AbstractItemAdapter*()> m_fFactory;
    TreeTraversalReflector* q_ptr;

    // Tests
    void _test_validateTree(TreeTraversalItems* p);

public Q_SLOTS:
    void cleanup();
    void slotRowsInserted  (const QModelIndex& parent, int first, int last);
    void slotRowsRemoved   (const QModelIndex& parent, int first, int last);
    void slotLayoutChanged (                                              );
    void slotRowsMoved     (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);
    void slotRowsMoved2    (const QModelIndex &p, int start, int end,
                            const QModelIndex &dest, int row);
};

#include <treetraversalreflector_debug.h>

#define S TreeTraversalItems::State::
const TreeTraversalItems::State TreeTraversalItems::m_fStateMap[6][7] = {
/*                 SHOW         HIDE        ATTACH     DETACH      UPDATE       MOVE         RESET    */
/*BUFFER   */ { S VISIBLE, S BUFFER   , S REACHABLE, S DANGLING , S BUFFER , S BUFFER   , S BUFFER    },
/*REMOVED  */ { S ERROR  , S ERROR    , S ERROR    , S BUFFER   , S ERROR  , S ERROR    , S ERROR     },
/*REACHABLE*/ { S VISIBLE, S REACHABLE, S ERROR    , S BUFFER   , S ERROR  , S REACHABLE, S REACHABLE },
/*VISIBLE  */ { S VISIBLE, S REACHABLE, S ERROR    , S BUFFER   , S VISIBLE, S VISIBLE  , S VISIBLE   },
/*ERROR    */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR     },
/*DANGLING */ { S ERROR  , S ERROR    , S ERROR    , S ERROR    , S ERROR  , S ERROR    , S ERROR     },
};
#undef S

#define A &TreeTraversalItems::
const TreeTraversalItems::StateF TreeTraversalItems::m_fStateMachine[6][7] = {
/*                 SHOW       HIDE      ATTACH    DETACH      UPDATE    MOVE     RESET  */
/*BUFFER   */ { A show   , A nothing, A attach, A destroy , A refresh, A index, A reset },
/*REMOVED  */ { A error  , A error  , A error , A detach  , A error  , A error, A reset },
/*REACHABLE*/ { A show   , A nothing, A error , A detach  , A error  , A index, A reset },
/*VISIBLE  */ { A nothing, A hide   , A error , A detach  , A refresh, A index, A reset },
/*ERROR    */ { A error  , A error  , A error , A error   , A error  , A error, A error },
/*DANGLING */ { A error  , A error  , A error , A error   , A error  , A error, A error },
};
#undef A

TreeTraversalItems* TreeTraversalItems::up() const
{
    TreeTraversalItems* ret = nullptr;

    // Another simple case, there is no parent
    if (!m_pParent) {
        Q_ASSERT(!m_Index.parent().isValid()); //TODO remove, no longer true when partial loading is implemented

        return nullptr;
    }

    // The parent is the element above in a Cartesian plan
    if (d_ptr->m_pRoot != m_pParent && !m_tSiblings[PREVIOUS])
        return m_pParent;

    ret = m_tSiblings[PREVIOUS];

    while (ret && ret->m_tChildren[LAST])
        ret = ret->m_tChildren[LAST];

    return ret;
}

TreeTraversalItems* TreeTraversalItems::down() const
{
    TreeTraversalItems* ret = nullptr;
    auto i = this;

    if (m_tChildren[FIRST]) {
        //Q_ASSERT(m_pParent->m_tChildren[FIRST]->m_Index.row() == 0); //racy
        ret = m_tChildren[FIRST];
        return ret;
    }


    // Recursively unwrap the tree until an element is found
    while(i) {
        if (i->m_tSiblings[NEXT] && (ret = i->m_tSiblings[NEXT]))
            return ret;

        i = i->m_pParent;
    }

    // Can't happen, exists to detect corrupted code
    if (m_Index.parent().isValid()) {
        Q_ASSERT(m_pParent);
//         Q_ASSERT(m_pParent->m_pParent->m_pParent->m_hLookup.size()
//             == m_pParent->m_Index.parent().row()+1);
    }

    return ret;
}

TreeTraversalItems* TreeTraversalItems::left() const
{
    return nullptr; //TODO
}

TreeTraversalItems* TreeTraversalItems::right() const
{
    return nullptr; //TODO
}


bool TreeTraversalItems::performAction(Action a)
{
    const int s = (int)m_State;
    m_State     = m_fStateMap            [s][(int)a];
    bool ret    = (this->*m_fStateMachine[s][(int)a])();

    return ret;
}

bool TreeTraversalItems::nothing()
{ return true; }


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsuggest-attribute=noreturn"
bool TreeTraversalItems::error()
{
    Q_ASSERT(false);
    return false;
}
#pragma GCC diagnostic pop


bool TreeTraversalItems::updateVisibility()
{

    //TODO support horizontal visibility

//     const auto sh = sizeHint();

    const bool isVisible = true; //FIXME

    //TODO this is a cheap workaround, it leaves the m_tVisibleTTIRange in a
    // potentially broken state
    if ((!m_pTreeItem) && !isVisible)
        return false;

    return isVisible;
}

bool TreeTraversalItems::show()
{
//     qDebug() << "SHOW";

    if (!m_pTreeItem) {
        Q_ASSERT(d_ptr->q_ptr->d_ptr->m_fFactory);
        m_pTreeItem = d_ptr->q_ptr->d_ptr->m_fFactory()->s_ptr;
        Q_ASSERT(m_pTreeItem);
        m_pTreeItem->m_pTTI = this;

        m_pTreeItem->performAction(VisualTreeItem::ViewAction::ATTACH);
    }
    m_pTreeItem->performAction(VisualTreeItem::ViewAction::ENTER_BUFFER);
    m_pTreeItem->performAction(VisualTreeItem::ViewAction::ENTER_VIEW  );

    // For some reason creating the visual element failed, this can and will
    // happen and need to be recovered from.
    if (m_pTreeItem->hasFailed()) {
        m_pTreeItem->performAction(VisualTreeItem::ViewAction::LEAVE_BUFFER);
        m_pTreeItem = nullptr;
    }

    updateVisibility();

    return true;
}

bool TreeTraversalItems::hide()
{
//     qDebug() << "HIDE";
    return true;
}

bool TreeTraversalItems::attach()
{
//     if (!updateVisibility())
//         return true;

    if (m_pTreeItem)
        m_pTreeItem->performAction(VisualTreeItem::ViewAction::ATTACH);


    //     qDebug() << "ATTACH" << (int)m_State;
    performAction(Action::MOVE); //FIXME don't
    return performAction(Action::SHOW); //FIXME don't
}

bool TreeTraversalItems::detach()
{
    // First, detach any remaining children
    auto i = m_hLookup.begin();
    while ((i = m_hLookup.begin()) != m_hLookup.end())
        i.value()->performAction(TreeTraversalItems::Action::DETACH);
    Q_ASSERT(m_hLookup.isEmpty());

    if (m_pTreeItem) {

        // If the item was active (due, for example, of a full reset), then
        // it has to be removed from view then deleted.
        if (m_pTreeItem->m_State == VisualTreeItem::State::ACTIVE) {
            m_pTreeItem->performAction(VisualTreeItem::ViewAction::DETACH);

            // It should still exists, it may crash otherwise, so make sure early
            Q_ASSERT(m_pTreeItem->m_State == VisualTreeItem::State::POOLED);

            m_pTreeItem->m_State = VisualTreeItem::State::POOLED;
            //FIXME ^^ add a new action for finish pooling or call
            // VisualTreeItem::detach from a new action method (instead of directly)
        }

        m_pTreeItem->performAction(VisualTreeItem::ViewAction::DETACH);
        m_pTreeItem = nullptr;
    }

    if (m_pParent) {
        const int size = m_pParent->m_hLookup.size();
        m_pParent->m_hLookup.remove(m_Index);
        Q_ASSERT(size == m_pParent->m_hLookup.size()+1);
    }

    if (m_tSiblings[PREVIOUS] || m_tSiblings[NEXT]) {
        if (m_tSiblings[PREVIOUS])
            m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] = m_tSiblings[NEXT];
        if (m_tSiblings[NEXT])
            m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = m_tSiblings[PREVIOUS];
    }
    else if (m_pParent) { //FIXME very wrong
        Q_ASSERT(m_pParent->m_hLookup.isEmpty());
        m_pParent->m_tChildren[FIRST] = nullptr;
        m_pParent->m_tChildren[LAST] = nullptr;
    }

    //FIXME set the parent m_tChildren[FIRST] correctly and add an insert()/move() method
    // then drop bridgeGap

    return true;
}

bool TreeTraversalItems::refresh()
{
    //
//     qDebug() << "REFRESH";

    for (auto i = m_tChildren[FIRST]; i; i = i->m_tSiblings[NEXT]) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->performAction(TreeTraversalItems::Action::UPDATE);

        if (i == m_tChildren[LAST])
            break;
    }

    //FIXME don't call directly
    if (!m_pTreeItem)
        show();

    return true;
}

bool TreeTraversalItems::index()
{
    //TODO remove this implementation and use the one below

    // Propagate
    for (auto i = m_tChildren[FIRST]; i; i = i->m_tSiblings[NEXT]) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->performAction(TreeTraversalItems::Action::MOVE);

        if (i == m_tChildren[LAST])
            break;
    }

    //FIXME this if should not exists, this should be handled by the state
    // machine.
    if (m_pTreeItem) {
        m_pTreeItem->performAction(VisualTreeItem::ViewAction::MOVE); //FIXME don't
        updateVisibility(); //FIXME add a new state change for this
    }

    return true;

//     if (!m_pTreeItem)
//         return true;

//FIXME this is better, but require having createItem() called earlier
//     if (m_pTreeItem)
//         m_pTreeItem->performAction(VisualTreeItem::ViewAction::MOVE); //FIXME don't
//
//     if (oldGeo != m_pTreeItem->geometry())
//         if (auto n = m_pTreeItem->down())
//             n->m_pParent->performAction(TreeTraversalItems::Action::MOVE);

//     return true;
}

bool TreeTraversalItems::destroy()
{
    detach();

    m_pTreeItem = nullptr;

    Q_ASSERT(m_hLookup.isEmpty());

    delete this;
    return true;
}

bool TreeTraversalItems::reset()
{
    for (auto i = m_tChildren[FIRST]; i; i = i->m_tSiblings[NEXT]) {
        Q_ASSERT(i);
        Q_ASSERT(i != this);
        i->performAction(TreeTraversalItems::Action::RESET);

        if (i == m_tChildren[LAST])
            break;
    }

    if (m_pTreeItem) {
        Q_ASSERT(this != d_ptr->m_pRoot);
        m_pTreeItem->performAction(VisualTreeItem::ViewAction::LEAVE_BUFFER);
        m_pTreeItem->performAction(VisualTreeItem::ViewAction::DETACH);
        m_pTreeItem = nullptr;
    }

    return this == d_ptr->m_pRoot ?
        true : performAction(TreeTraversalItems::Action::UPDATE);
}

TreeTraversalReflector::TreeTraversalReflector(QObject* parent) : QObject(parent),
    d_ptr(new TreeTraversalReflectorPrivate())
{
    d_ptr->q_ptr = this;
}

TreeTraversalReflector::~TreeTraversalReflector()
{
    delete d_ptr->m_pRoot;
}

// Setters
void TreeTraversalReflector::setItemFactory(std::function<AbstractItemAdapter*()> factory)
{
    d_ptr->m_fFactory = factory;
}

void TreeTraversalReflectorPrivate::slotRowsInserted(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());
//     qDebug() << "\n\nADD" << first << last;

    if (!q_ptr->isActive(parent, first, last)) {
        Q_ASSERT(false); //FIXME so I can't forget when time comes
        return;
    }

//     qDebug() << "\n\nADD2" << q_ptr->width() << q_ptr->height();

    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    TreeTraversalItems *prev(nullptr);

    //FIXME use up()
    if (first && pitem)
        prev = pitem->m_hLookup.value(q_ptr->model()->index(first-1, 0, parent));

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);
        Q_ASSERT(idx.isValid());
        Q_ASSERT(idx.parent() != idx);
        Q_ASSERT(idx.model() == q_ptr->model());

        auto e = addChildren(pitem, idx);

        // Keep a dual chained linked list between the visual elements
        e->m_tSiblings[PREVIOUS] = prev ? prev : nullptr; //FIXME incorrect

        //FIXME It can happen if the previous is out of the visible range
        Q_ASSERT( e->m_tSiblings[PREVIOUS] || e->m_Index.row() == 0);

        //TODO merge with bridgeGap
        if (prev)
            bridgeGap(prev, e, true);

        // This is required before ::ATTACH because otherwise ::down() wont work
        if ((!pitem->m_tChildren[FIRST]) || e->m_Index.row() <= pitem->m_tChildren[FIRST]->m_Index.row()) {
            e->m_tSiblings[NEXT] = pitem->m_tChildren[FIRST];
            pitem->m_tChildren[FIRST] = e;
        }

        e->performAction(TreeTraversalItems::Action::ATTACH);

        if ((!pitem->m_tChildren[FIRST]) || e->m_Index.row() <= pitem->m_tChildren[FIRST]->m_Index.row()) {
            Q_ASSERT(pitem != e);
            if (auto pe = e->up())
                pe->performAction(TreeTraversalItems::Action::MOVE);
        }

        if ((!pitem->m_tChildren[LAST]) || e->m_Index.row() > pitem->m_tChildren[LAST]->m_Index.row()) {
            Q_ASSERT(pitem != e);
            pitem->m_tChildren[LAST] = e;
            if (auto ne = e->down())
                ne->performAction(TreeTraversalItems::Action::MOVE);
        }

        if (auto rc = q_ptr->model()->rowCount(idx))
            slotRowsInserted(idx, 0, rc-1);

        // Validate early to prevent propagating garbage that's nearly impossible
        // to debug.
        if (pitem && pitem != m_pRoot && !i) {
            Q_ASSERT(e->up() == pitem);
            Q_ASSERT(e == pitem->down());
        }

        prev = e;

    }

    if ((!pitem->m_tChildren[LAST]) || last > pitem->m_tChildren[LAST]->m_Index.row())
        pitem->m_tChildren[LAST] = prev;

    Q_ASSERT(pitem->m_tChildren[LAST]);

    //FIXME use down()
    if (q_ptr->model()->rowCount(parent) > last) {
        if (auto i = pitem->m_hLookup.value(q_ptr->model()->index(last+1, 0, parent))) {
            i->m_tSiblings[PREVIOUS] = prev;
            prev->m_tSiblings[NEXT] = i;
        }
//         else //FIXME it happens
//             Q_ASSERT(false);
    }

    //FIXME EVIL and useless
    m_pRoot->performAction(TreeTraversalItems::Action::MOVE);

    _test_validateTree(m_pRoot);

    Q_EMIT q_ptr->contentChanged();

    if (!parent.isValid())
        Q_EMIT q_ptr->countChanged();
}

void TreeTraversalReflectorPrivate::slotRowsRemoved(const QModelIndex& parent, int first, int last)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());
    Q_EMIT q_ptr->contentChanged();

    if (!q_ptr->isActive(parent, first, last))
        return;

    auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;

    //TODO make sure the state machine support them
    //TreeTraversalItems *prev(nullptr), *next(nullptr);

    //FIXME use up()
    //if (first && pitem)
    //    prev = pitem->m_hLookup.value(model()->index(first-1, 0, parent));

    //next = pitem->m_hLookup.value(model()->index(last+1, 0, parent));

    //FIXME support smaller ranges
    for (int i = first; i <= last; i++) {
        auto idx = q_ptr->model()->index(i, 0, parent);

        auto elem = pitem->m_hLookup.value(idx);
        Q_ASSERT(elem);

        elem->performAction(TreeTraversalItems::Action::DETACH);
    }

    if (!parent.isValid())
        Q_EMIT q_ptr->countChanged();
}

void TreeTraversalReflectorPrivate::slotLayoutChanged()
{

    if (auto rc = q_ptr->model()->rowCount())
        slotRowsInserted({}, 0, rc - 1);

    Q_EMIT q_ptr->contentChanged();

    Q_EMIT q_ptr->countChanged();
}

void TreeTraversalReflectorPrivate::createGap(TreeTraversalItems* first, TreeTraversalItems* last)
{
    Q_ASSERT(first->m_pParent == last->m_pParent);

    if (first->m_tSiblings[PREVIOUS]) {
        first->m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] = last->m_tSiblings[NEXT];
    }

    if (last->m_tSiblings[NEXT]) {
        last->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = first->m_tSiblings[PREVIOUS];
    }

    if (first->m_pParent->m_tChildren[FIRST] == first)
        first->m_pParent->m_tChildren[FIRST] = last->m_tSiblings[NEXT];

    if (last->m_pParent->m_tChildren[LAST] == last)
        last->m_pParent->m_tChildren[LAST] = first->m_tSiblings[PREVIOUS];

    Q_ASSERT((!first->m_tSiblings[PREVIOUS]) ||
        first->m_tSiblings[PREVIOUS]->down() != first);
    Q_ASSERT((!last->m_tSiblings[NEXT]) ||
        last->m_tSiblings[NEXT]->up() != last);

    Q_ASSERT((!first) || first->m_tChildren[FIRST] || first->m_hLookup.isEmpty());
    Q_ASSERT((!last) || last->m_tChildren[FIRST] || last->m_hLookup.isEmpty());

    // Do not leave invalid pointers for easier debugging
    last->m_tSiblings[NEXT]      = nullptr;
    first->m_tSiblings[PREVIOUS] = nullptr;
}

/// Fix the issues introduced by createGap (does not update m_pParent and m_hLookup)
void TreeTraversalReflectorPrivate::bridgeGap(TreeTraversalItems* first, TreeTraversalItems* second, bool insert)
{
    // 3 possible case: siblings, first child or last child

    if (first && second && first->m_pParent == second->m_pParent) {
        // first and second are siblings

        // Assume the second item is new
        if (insert && first->m_tSiblings[NEXT]) {
            second->m_tSiblings[NEXT] = first->m_tSiblings[NEXT];
            first->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] = second;
        }

        first->m_tSiblings[NEXT] = second;
        second->m_tSiblings[PREVIOUS] = first;
    }
    else if (second && ((!first) || first == second->m_pParent)) {
        // The `second` is `first` first child or it's the new root
        second->m_tSiblings[PREVIOUS] = nullptr;

        if (!second->m_pParent->m_tChildren[LAST])
            second->m_pParent->m_tChildren[LAST] = second;

        second->m_tSiblings[NEXT] = second->m_pParent->m_tChildren[FIRST];

        if (second->m_pParent->m_tChildren[FIRST]) {
            second->m_pParent->m_tChildren[FIRST]->m_tSiblings[PREVIOUS] = second;
        }

        second->m_pParent->m_tChildren[FIRST] = second;

        //BEGIN test
        /*int count =0;
        for (auto c = second->m_pParent->m_tChildren[FIRST]; c; c = c->m_tSiblings[NEXT])
            count++;
        Q_ASSERT(count == second->m_pParent->m_hLookup.size());*/
        //END test
    }
    else if (first) {
        // It's the last element or the second is a last leaf and first is unrelated
        first->m_tSiblings[NEXT] = nullptr;

        if (!first->m_pParent->m_tChildren[FIRST])
            first->m_pParent->m_tChildren[FIRST] = first;

        if (first->m_pParent->m_tChildren[LAST] && first->m_pParent->m_tChildren[LAST] != first) {
            first->m_pParent->m_tChildren[LAST]->m_tSiblings[NEXT] = first;
            first->m_tSiblings[PREVIOUS] = first->m_pParent->m_tChildren[LAST];
        }

        first->m_pParent->m_tChildren[LAST] = first;

        //BEGIN test
        int count =0;
        for (auto c = first->m_pParent->m_tChildren[LAST]; c; c = c->m_tSiblings[PREVIOUS])
            count++;

        Q_ASSERT(first->m_pParent->m_tChildren[FIRST]);

        Q_ASSERT(count == first->m_pParent->m_hLookup.size());
        //END test
    }
    else {
        Q_ASSERT(false); //Something went really wrong elsewhere
    }

    if (first)
        Q_ASSERT(first->m_pParent->m_tChildren[FIRST]);
    if (second)
        Q_ASSERT(second->m_pParent->m_tChildren[FIRST]);

    if (first)
        Q_ASSERT(first->m_pParent->m_tChildren[LAST]);
    if (second)
        Q_ASSERT(second->m_pParent->m_tChildren[LAST]);

//     if (first && second) { //Need to disable other asserts in down()
//         Q_ASSERT(first->down() == second);
//         Q_ASSERT(second->up() == first);
//     }
}

void TreeTraversalReflectorPrivate::setTemporaryIndices(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    //FIXME list only
    // Before moving them, set a temporary now/col value because it wont be set
    // on the index until before slotRowsMoved2 is called (but after this
    // method returns //TODO do not use the hashmap, it is already known
    if (parent == destination) {
        const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;
        for (int i = start; i <= end; i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);

            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);

            elem->m_MoveToRow = row + (i - start);
        }

        for (int i = row; i <= row + (end - start); i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);

            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);

            elem->m_MoveToRow = row + (end - start) + 1;
        }
    }
}

void TreeTraversalReflectorPrivate::resetTemporaryIndices(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    //FIXME list only
    // Before moving them, set a temporary now/col value because it wont be set
    // on the index until before slotRowsMoved2 is called (but after this
    // method returns //TODO do not use the hashmap, it is already known
    if (parent == destination) {
        const auto pitem = parent.isValid() ? m_hMapper.value(parent) : m_pRoot;
        for (int i = start; i <= end; i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);
            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);
            elem->m_MoveToRow = -1;
        }

        for (int i = row; i <= row + (end - start); i++) {
            auto idx = q_ptr->model()->index(i, 0, parent);
            auto elem = pitem->m_hLookup.value(idx);
            Q_ASSERT(elem);
            elem->m_MoveToRow = -1;
        }
    }
}

void TreeTraversalReflectorPrivate::slotRowsMoved(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    Q_ASSERT((!parent.isValid()) || parent.model() == q_ptr->model());
    Q_ASSERT((!destination.isValid()) || destination.model() == q_ptr->model());

    // There is literally nothing to do
    if (parent == destination && start == row)
        return;

    // Whatever has to be done only affect a part that's not currently tracked.
    if ((!q_ptr->isActive(parent, start, end)) && !q_ptr->isActive(destination, row, row+(end-start)))
        return;

    setTemporaryIndices(parent, start, end, destination, row);

    //TODO also support trees

    // As the actual view is implemented as a daisy chained list, only moving
    // the edges is necessary for the TreeTraversalItems. Each VisualTreeItem
    // need to be moved.

    const auto idxStart = q_ptr->model()->index(start, 0, parent);
    const auto idxEnd   = q_ptr->model()->index(end  , 0, parent);
    Q_ASSERT(idxStart.isValid() && idxEnd.isValid());

    //FIXME once partial ranges are supported, this is no longer always valid
    auto startTTI = ttiForIndex(idxStart);
    auto endTTI   = ttiForIndex(idxEnd);

    if (end - start == 1)
        Q_ASSERT(startTTI->m_tSiblings[NEXT] == endTTI);

    //FIXME so I don't forget, it will mess things up if silently ignored
    Q_ASSERT(startTTI && endTTI);
    Q_ASSERT(startTTI->m_pParent == endTTI->m_pParent);

    auto oldPreviousTTI = startTTI->up();
    auto oldNextTTI     = endTTI->down();

    Q_ASSERT((!oldPreviousTTI) || oldPreviousTTI->down() == startTTI);
    Q_ASSERT((!oldNextTTI) || oldNextTTI->up() == endTTI);

    auto newNextIdx = q_ptr->model()->index(row, 0, destination);

    // You cannot move things into an empty model
    Q_ASSERT((!row) || newNextIdx.isValid());

    TreeTraversalItems *newNextTTI(nullptr), *newPrevTTI(nullptr);

    // Rewind until a next element is found, this happens when destination is empty
    if (!newNextIdx.isValid() && destination.parent().isValid()) {
        Q_ASSERT(q_ptr->model()->rowCount(destination) == row);
        auto par = destination.parent();
        do {
            if (q_ptr->model()->rowCount(par.parent()) > par.row()) {
                newNextIdx = q_ptr->model()->index(par.row(), 0, par.parent());
                break;
            }

            par = par.parent();
        } while (par.isValid());

        newNextTTI = ttiForIndex(newNextIdx);
    }
    else {
        newNextTTI = ttiForIndex(newNextIdx);
        newPrevTTI = newNextTTI ? newNextTTI->up() : nullptr;
    }

    if (!row) {
        auto otherI = ttiForIndex(destination);
        Q_ASSERT((!newPrevTTI) || otherI == newPrevTTI);
    }

    // When there is no next element, then the parent has to be extracted manually
    if (!(newNextTTI || newPrevTTI)) {
        if (!row)
            newPrevTTI = ttiForIndex(destination);
        else {
            newPrevTTI = ttiForIndex(
                destination.model()->index(row-1, 0, destination)
            );
        }
    }

    Q_ASSERT((newPrevTTI || startTTI) && newPrevTTI != startTTI);
    Q_ASSERT((newNextTTI || endTTI  ) && newNextTTI != endTTI  );

    TreeTraversalItems* newParentTTI = ttiForIndex(destination);
    newParentTTI = newParentTTI ? newParentTTI : m_pRoot;
    auto oldParentTTI = startTTI->m_pParent;

    // Make sure not to leave invalid pointers while the steps below are being performed
    createGap(startTTI, endTTI);

    // Update the tree parent (if necessary)
    if (oldParentTTI != newParentTTI) {
        for (auto i = startTTI; i; i = i->m_tSiblings[NEXT]) {
            auto idx = i->m_Index;

            const int size = oldParentTTI->m_hLookup.size();
            oldParentTTI->m_hLookup.remove(idx);
            Q_ASSERT(oldParentTTI->m_hLookup.size() == size-1);

            newParentTTI->m_hLookup[idx] = i;
            i->m_pParent = newParentTTI;
            if (i == endTTI)
                break;
        }
    }

    Q_ASSERT(startTTI->m_pParent == newParentTTI);
    Q_ASSERT(endTTI->m_pParent   == newParentTTI);

    bridgeGap(newPrevTTI, startTTI );
    bridgeGap(endTTI   , newNextTTI);

    // Close the gap between the old previous and next elements
    Q_ASSERT(startTTI->m_tSiblings[NEXT]     != startTTI);
    Q_ASSERT(startTTI->m_tSiblings[PREVIOUS] != startTTI);
    Q_ASSERT(endTTI->m_tSiblings[NEXT]       != endTTI  );
    Q_ASSERT(endTTI->m_tSiblings[PREVIOUS]   != endTTI  );

    //BEGIN debug
    if (newPrevTTI) {
        int count = 0;
        for (auto c = newPrevTTI->m_pParent->m_tChildren[FIRST]; c; c = c->m_tSiblings[NEXT])
            count++;
        Q_ASSERT(count == newPrevTTI->m_pParent->m_hLookup.size());

        count = 0;
        for (auto c = newPrevTTI->m_pParent->m_tChildren[LAST]; c; c = c->m_tSiblings[PREVIOUS])
            count++;
        Q_ASSERT(count == newPrevTTI->m_pParent->m_hLookup.size());
    }
    //END

    bridgeGap(oldPreviousTTI, oldNextTTI);


    if (endTTI->m_tSiblings[NEXT]) {
        Q_ASSERT(endTTI->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] == endTTI);
    }

    if (startTTI->m_tSiblings[PREVIOUS]) {
        Q_ASSERT(startTTI->m_tSiblings[PREVIOUS]->m_pParent == startTTI->m_pParent);
        Q_ASSERT(startTTI->m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] == startTTI);
    }


//     Q_ASSERT((!newNextVI) || newNextVI->m_pParent->m_tSiblings[PREVIOUS] == endVI->m_pParent);
//     Q_ASSERT((!newPrevVI) ||
// //         newPrevVI->m_pParent->m_tSiblings[NEXT] == startVI->m_pParent ||
//         (newPrevVI->m_pParent->m_tChildren[FIRST] == startVI->m_pParent && !row)
//     );

//     Q_ASSERT((!oldPreviousVI) || (!oldPreviousVI->m_pParent->m_tSiblings[NEXT]) ||
//         oldPreviousVI->m_pParent->m_tSiblings[NEXT] == (oldNextVI ? oldNextVI->m_pParent : nullptr));


    // Move everything
    //TODO move it more efficient
    m_pRoot->performAction(TreeTraversalItems::Action::MOVE);

    resetTemporaryIndices(parent, start, end, destination, row);
}

void TreeTraversalReflectorPrivate::slotRowsMoved2(const QModelIndex &parent, int start, int end,
                                     const QModelIndex &destination, int row)
{
    Q_UNUSED(parent)
    Q_UNUSED(start)
    Q_UNUSED(end)
    Q_UNUSED(destination)
    Q_UNUSED(row)

    // The test would fail if it was in aboutToBeMoved
    _test_validateTree(m_pRoot);
}

void TreeTraversalReflector::resetEverything()
{
    // There is nothing to reset when there is no model
    if (!model())
        return;

    d_ptr->m_pRoot->performAction(TreeTraversalItems::Action::RESET);
}

QAbstractItemModel* TreeTraversalReflector::model() const
{
    return d_ptr->m_pModel;
}

void TreeTraversalReflector::setModel(QAbstractItemModel* m)
{
    if (m == model())
        return;

    if (auto oldM = model()) {
        disconnect(oldM, &QAbstractItemModel::rowsInserted, d_ptr,
            &TreeTraversalReflectorPrivate::slotRowsInserted);
        disconnect(oldM, &QAbstractItemModel::rowsAboutToBeRemoved, d_ptr,
            &TreeTraversalReflectorPrivate::slotRowsRemoved);
        disconnect(oldM, &QAbstractItemModel::layoutAboutToBeChanged, d_ptr,
            &TreeTraversalReflectorPrivate::cleanup);
        disconnect(oldM, &QAbstractItemModel::layoutChanged, d_ptr,
            &TreeTraversalReflectorPrivate::slotLayoutChanged);
        disconnect(oldM, &QAbstractItemModel::modelAboutToBeReset, d_ptr,
            &TreeTraversalReflectorPrivate::cleanup);
        disconnect(oldM, &QAbstractItemModel::modelReset, d_ptr,
            &TreeTraversalReflectorPrivate::slotLayoutChanged);
        disconnect(oldM, &QAbstractItemModel::rowsAboutToBeMoved, d_ptr,
            &TreeTraversalReflectorPrivate::slotRowsMoved);
        disconnect(oldM, &QAbstractItemModel::rowsMoved, d_ptr,
            &TreeTraversalReflectorPrivate::slotRowsMoved2);

        d_ptr->slotRowsRemoved({}, 0, oldM->rowCount()-1);
    }

    d_ptr->m_hMapper.clear();
    delete d_ptr->m_pRoot;
    d_ptr->m_pRoot = new TreeTraversalItems(nullptr, d_ptr);

    d_ptr->m_pModel = m;

    if (!m)
        return;

    connect(model(), &QAbstractItemModel::rowsInserted, d_ptr,
        &TreeTraversalReflectorPrivate::slotRowsInserted );
    connect(model(), &QAbstractItemModel::rowsAboutToBeRemoved, d_ptr,
        &TreeTraversalReflectorPrivate::slotRowsRemoved  );
    connect(model(), &QAbstractItemModel::layoutAboutToBeChanged, d_ptr,
        &TreeTraversalReflectorPrivate::cleanup);
    connect(model(), &QAbstractItemModel::layoutChanged, d_ptr,
        &TreeTraversalReflectorPrivate::slotLayoutChanged);
    connect(model(), &QAbstractItemModel::modelAboutToBeReset, d_ptr,
        &TreeTraversalReflectorPrivate::cleanup);
    connect(model(), &QAbstractItemModel::modelReset, d_ptr,
        &TreeTraversalReflectorPrivate::slotLayoutChanged);
    connect(model(), &QAbstractItemModel::rowsAboutToBeMoved, d_ptr,
        &TreeTraversalReflectorPrivate::slotRowsMoved);
    connect(model(), &QAbstractItemModel::rowsMoved, d_ptr,
        &TreeTraversalReflectorPrivate::slotRowsMoved2);
}

void TreeTraversalReflector::populate()
{
    if (!model())
        return;

    if (auto rc = model()->rowCount())
        d_ptr->slotRowsInserted({}, 0, rc - 1);
}

/// Return true if the indices affect the current view
bool TreeTraversalReflector::isActive(const QModelIndex& parent, int first, int last)
{
    Q_UNUSED(parent)
    Q_UNUSED(first)
    Q_UNUSED(last)

    return true; //FIXME

    /*if (m_State == State::UNFILLED)
        return true;

    //FIXME only insert elements with loaded children into d_ptr->m_hMapper
    auto pitem = parent.isValid() ? d_ptr->m_hMapper.value(parent) : d_ptr->m_pRoot;

    if (parent.isValid() && pitem == d_ptr->m_pRoot)
        return false;

    if ((!pitem->m_tChildren[LAST]) || (!pitem->m_tChildren[FIRST]))
        return true;

    if (pitem->m_tChildren[LAST]->m_Index.row() >= first)
        return true;

    if (pitem->m_tChildren[FIRST]->m_Index.row() <= last)
        return true;

    return false;*/
}

/// Add new entries to the mapping
TreeTraversalItems* TreeTraversalReflectorPrivate::addChildren(TreeTraversalItems* parent, const QModelIndex& index)
{
    Q_ASSERT(index.isValid());
    Q_ASSERT(index.parent() != index);

    auto e = new TreeTraversalItems(parent, this);
    e->m_Index = index;

    const int oldSize(m_hMapper.size()), oldSize2(parent->m_hLookup.size());

    m_hMapper [index] = e;
    parent->m_hLookup[index] = e;

    // If the size did not grow, something leaked
    Q_ASSERT(m_hMapper.size() == oldSize+1);
    Q_ASSERT(parent->m_hLookup.size() == oldSize2+1);

    return e;
}

void TreeTraversalReflectorPrivate::cleanup()
{
    m_pRoot->performAction(TreeTraversalItems::Action::DETACH);

    m_hMapper.clear();
    m_pRoot = new TreeTraversalItems(nullptr, this);
    //m_FailedCount = 0;
}

TreeTraversalItems* TreeTraversalReflectorPrivate::ttiForIndex(const QModelIndex& idx) const
{
    if (!idx.isValid())
        return nullptr;

    if (!idx.parent().isValid())
        return m_pRoot->m_hLookup.value(idx);

    if (auto parent = m_hMapper.value(idx.parent()))
        return parent->m_hLookup.value(idx);

    return nullptr;
}

AbstractItemAdapter* TreeTraversalReflector::itemForIndex(const QModelIndex& idx) const
{
    const auto tti = d_ptr->ttiForIndex(idx);
    return tti && tti->m_pTreeItem ? tti->m_pTreeItem->d_ptr : nullptr;
}

QPersistentModelIndex VisualTreeItem::index() const
{
    return m_pTTI->m_Index;
}

/**
 * Flatten the tree as a linked list.
 *
 * Returns the previous non-failed item.
 */
VisualTreeItem* VisualTreeItem::up() const
{
    Q_ASSERT(m_State == State::ACTIVE
        || m_State == State::BUFFER
        || m_State == State::FAILED
        || m_State == State::POOLING
        || m_State == State::DANGLING //FIXME add a new state for
        // "deletion in progress" or swap the f call and set state
    );

    auto ret = m_pTTI->up();
    //TODO support collapsed nodes

    // Linearly look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    while (ret && !ret->m_pTreeItem)
        ret = ret->up();

    return ret ? ret->m_pTreeItem : nullptr;
}

/**
 * Flatten the tree as a linked list.
 *
 * Returns the next non-failed item.
 */
VisualTreeItem* VisualTreeItem::down() const
{
    Q_ASSERT(m_State == State::ACTIVE
        || m_State == State::BUFFER
        || m_State == State::FAILED
        || m_State == State::POOLING
        || m_State == State::DANGLING //FIXME add a new state for
        // "deletion in progress" or swap the f call and set state
    );

    auto ret = m_pTTI->down();
    //TODO support collapsed entries

    // Recursively look for a valid element. Doing this here allows the views
    // that implement this (abstract) class to work without having to always
    // check if some of their item failed to load. This is non-fatal in the
    // other Qt views, so it isn't fatal here either.
    while (ret && !ret->m_pTreeItem)
        ret = ret->down();

    return ret ? ret->m_pTreeItem : nullptr;
}

int VisualTreeItem::row() const
{
    return m_pTTI->m_MoveToRow == -1 ?
        index().row() : m_pTTI->m_MoveToRow;
}

int VisualTreeItem::column() const
{
    return m_pTTI->m_MoveToColumn == -1 ?
        index().column() : m_pTTI->m_MoveToColumn;
}

//TODO remove
void TreeTraversalReflector::moveEverything()
{
    d_ptr->m_pRoot->performAction(TreeTraversalItems::Action::MOVE);
}

#undef PREVIOUS
#undef NEXT
#undef FIRST
#undef LAST

#include <treetraversalreflector_p.moc>
