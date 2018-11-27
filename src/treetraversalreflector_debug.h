/**
 * While this module is under development and the autotests are lacking,
 * always run strict tests at runtime.
 *
 * This is 3 ways to look at the same data:
 *
 *  * As a tree
 *  * As a linked list
 *  * As a "sliding window" (viewport)
 */

void TreeTraversalReflectorPrivate::_test_validateTree(StateTracker::ModelItem* p)
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif

    // The asserts below only work on valid models with valid delegates.
    // If those conditions are not met, it *could* work anyway, but cannot be
    // validated.
    /*Q_ASSERT(m_FailedCount >= 0);
    if (m_FailedCount) {
        qWarning() << "The tree is fragmented and failed to self heal: disable validation";
        return;
    }*/

//     if (p->parent() == m_pRoot && m_pRoot->firstChild() == p && p->m_Geometry.viewTracker()) {
//         Q_ASSERT(!p->m_Geometry.viewTracker()->up());
//     }

    // First, let's check the linked list to avoid running more test on really
    // corrupted data
    if (auto i = p->firstChild()) {
        auto idx = i->index();
        int count = 1;
        auto oldI = i;

        Q_ASSERT(idx.isValid());

        while ((oldI = i) && (i = i->nextSibling())) {
            // If this is a next, then there has to be a previous
            Q_ASSERT(i->parent() == p);
            Q_ASSERT(i->previousSibling());
            Q_ASSERT(i->previousSibling()->index() == idx);
            //Q_ASSERT(i->effectiveRow() == idx.row()+1); //FIXME
            Q_ASSERT(i->previousSibling()->nextSibling() == i);
            Q_ASSERT(i->previousSibling() == oldI);
            Q_ASSERT((!oldI->index().isValid()) || i->index().parent() == oldI->index().parent());
            Q_ASSERT((!oldI->index().isValid()) || i->effectiveRow() == oldI->effectiveRow()+1);
            idx = i->index();
            Q_ASSERT(idx.isValid());
            count++;
        }

        Q_ASSERT(p->lastChild());
        Q_ASSERT(p == p->firstChild()->parent());
        Q_ASSERT(p == p->lastChild()->parent());
        Q_ASSERT(p->loadedChildrenCount() == count);
    }

    // Do that again in the other direction
    if (auto i = p->lastChild()) {
        auto idx = i->index();
        auto oldI = i;
        int count = 1;

        Q_ASSERT(idx.isValid());

        while ((oldI = i) && (i = i->previousSibling())) {
            Q_ASSERT(i->nextSibling());
            Q_ASSERT(i->nextSibling()->index() == idx);
            Q_ASSERT(i->parent() == p);
            //Q_ASSERT(i->effectiveRow() == idx.row()-1); //FIXME
            Q_ASSERT(i->nextSibling()->previousSibling() == i);
            Q_ASSERT(i->nextSibling() == oldI);
            idx = i->index();
            Q_ASSERT(idx.isValid());
            count++;
        }

        Q_ASSERT(p->loadedChildrenCount() == count);
    }

    //TODO remove once stable
    // Brute force recursive validations
    StateTracker::ModelItem *old(nullptr), *newest(nullptr);

    const auto children = p->allLoadedChildren();

    for (auto i : qAsConst(children)) {
        Q_ASSERT(i->parent() == p);
        StateTracker::ModelItem *item = TTI(i);
        if ((!newest) || i->effectiveRow() < newest->effectiveRow())
            newest = TTI(item);

        if ((!old) || i->effectiveRow() > old->effectiveRow())
            old = TTI(item);

        // Check that m_FailedCount is valid
        //Q_ASSERT(!item->m_Geometry.viewTracker()->hasFailed());

        // Test the indices
        Q_ASSERT(p == m_pRoot || i->index().internalPointer() == item->index().internalPointer());
        Q_ASSERT(p == m_pRoot || (p->index().isValid()) || p->index().internalPointer() != i->index().internalPointer());
        //Q_ASSERT(old == item || old->effectiveRow() > i->effectiveRow()); //FIXME
        //Q_ASSERT(newest == item || newest->effectiveRow() < i->effectiveRow()); //FIXME

        // Test that there is no trivial duplicate StateTracker::ModelItem for the same index
        if(item->previousSibling() && !item->previousSibling()->loadedChildrenCount()) {
            const auto prev = item->up();
            Q_ASSERT(prev == item->previousSibling());

            const auto next = prev->down();
            Q_ASSERT(next == item);

            if (prev == item->parent()) {
                Q_ASSERT(item->effectiveRow() == 0);
                Q_ASSERT(item->effectiveParentIndex() == prev->index());
            }
        }

        // Test the virtual linked list between the leafs and branches
        if(auto next = item->down()) {
            Q_ASSERT(next->up() == item);
            Q_ASSERT(next != item);

            if (next->effectiveParentIndex() == item->effectiveParentIndex()) {
                const int rc = m_pModel->rowCount(item->index());
                Q_ASSERT(!rc);
            }
        }
        else {
            // There is always a next is those conditions are not met unless there
            // is failed elements creating (auto-corrected) holes in the chains.
            Q_ASSERT(!item->nextSibling());
            Q_ASSERT(!item->loadedChildrenCount());
        }

        if(auto prev = item->up()) {
            Q_ASSERT(prev->down() == item);
            Q_ASSERT(prev != item);
        }
        else {
            // There is always a previous if those conditions are not met unless there
            // is failed elements creating (auto-corrected) holes in the chains.
            Q_ASSERT(!item->previousSibling());
            Q_ASSERT(item->parent() == m_pRoot);
        }

        _test_validateTree(TTI(item));
    }

    // Traverse as a list
    if (p == m_pRoot) {
        StateTracker::Index* oldTTI(nullptr);

        int count(0), count2(0);
        for (auto i = m_pRoot->firstChild(); i; i = i->down()) {
            Q_ASSERT((!oldTTI) || i->up());
            Q_ASSERT(i->up() == oldTTI);
            oldTTI = i;
            count++;
        }

        // Backward too
        oldTTI = nullptr;
        auto last = m_pRoot->lastChild();
        while (last && last->lastChild())
            last = last->lastChild();

        for (auto i = last; i; i = i->up()) {
            Q_ASSERT((!oldTTI) || i->down());
            Q_ASSERT(i->down() == oldTTI);
            oldTTI = i;
            count2++;
        }

        Q_ASSERT(count == count2);
    }

    // Test that the list edges are valid
    Q_ASSERT(!(!!p->lastChild() ^ !!p->firstChild()));
    Q_ASSERT(p->lastChild()  == old);
    Q_ASSERT(p->firstChild() == newest);
    Q_ASSERT((!old) || !old->nextSibling());
    Q_ASSERT((!newest) || !newest->previousSibling());
}

void TreeTraversalReflectorPrivate::_test_validateLinkedList(bool skipVItemState)
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    _test_validateTree(m_pRoot);

    if (!m_pRoot->firstChild()) {
        Q_ASSERT(!m_pRoot->lastChild());
        Q_ASSERT(!m_pRoot->loadedChildrenCount());
        Q_ASSERT(!m_pRoot->previousSibling());
        Q_ASSERT(!m_pRoot->nextSibling());
        return;
    }
    else {
        auto first = TTI(m_pRoot->firstChild());
        Q_ASSERT(first->m_Geometry.removeMe() != (int)StateTracker::Geometry::State::SIZE);
        Q_ASSERT(first->m_Geometry.removeMe() != (int)StateTracker::Geometry::State::INIT);
    }

    Q_ASSERT(!m_pRoot->firstChild()->up());

    StateTracker::ModelItem *prev(nullptr), *cur(TTI(m_pRoot->firstChild()));

    static const constexpr qreal maxReal = std::numeric_limits<qreal>::max();
    qreal maxY(0), maxX(0), minY(maxReal), minX(maxReal);

    bool hadVisible      = false;
    bool visibleFinished = false;

//     qDebug() << "";
//     while ((prev = cur) && (cur = TTI(cur->down()))) {
//         qDebug() << "TREE"
//             << (cur->m_State == StateTracker::ModelItem::State::VISIBLE);
//     }
//     qDebug() << "DONE";

    prev = nullptr;
    cur  = TTI(m_pRoot->firstChild());

    StateTracker::ModelItem *firstVisible(nullptr), *lastVisible(nullptr);

    _test_validate_edges_simple();

    const auto tve = edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
    const auto bve = edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);

//     qDebug() << "START";
    do {
        Q_ASSERT(cur->up() == prev);
        Q_ASSERT(cur->index().isValid());
        Q_ASSERT(cur->index().model() == cur->d_ptr->m_pModel);

        if (!visibleFinished) {
            // If hit, it means the visible rect wasn't refreshed.
            //Q_ASSERT(cur->m_Geometry.isValid()); //TODO THIS_COMMIT
        }
        else //FIXME wrong, only added to prevent forgetting
            Q_ASSERT(cur->m_Geometry.isValid());

        if (cur->m_State == StateTracker::ModelItem::State::VISIBLE && !hadVisible) {
            Q_ASSERT(!hadVisible);
            firstVisible = cur;
            hadVisible = true;
            Q_ASSERT(tve == cur);
        }
        else if (hadVisible && cur->m_State != StateTracker::ModelItem::State::VISIBLE) {
            visibleFinished = true;
            lastVisible = prev;
            Q_ASSERT(bve == lastVisible);
        }

//         if (cur->m_State == StateTracker::ModelItem::State::VISIBLE) {
//             Q_ASSERT(cur->m_Geometry.isInSync());
//         }

        Q_ASSERT(cur->m_State != StateTracker::ModelItem::State::VISIBLE || hadVisible);

        auto vi = cur->m_Geometry.viewTracker();

//         if (vi) {
//             if (vi->m_State == StateTracker::ViewItem::State::ACTIVE) {
//                 Q_ASSERT(cur->m_State == StateTracker::ModelItem::State::VISIBLE);
//                 Q_ASSERT(m_pViewport->currentRect().isValid());
//                 Q_ASSERT(m_pViewport->currentRect().intersects(
//                     vi->geometry()
//                 ));
//                 qDebug() << "LA!!!!";
//             }
//             else {
//                 Q_ASSERT(vi->m_State == StateTracker::ViewItem::State::BUFFER);
//                 Q_ASSERT(!m_pViewport->currentRect().intersects(
//                     vi->geometry()
//                 ));
//             }
//         }

        Q_ASSERT((!visibleFinished) || visibleFinished ^ (cur->m_State == StateTracker::ModelItem::State::VISIBLE));

        // skipVItemState is necessary to test some steps in between the tree and view
        if (!skipVItemState) {
            Q_ASSERT(cur->m_State == StateTracker::ModelItem::State::VISIBLE || !vi);
            Q_ASSERT((!visibleFinished) || !vi);
        }

        // Check the the previous sibling has no children
        if (prev && cur->parent() == prev->parent()) {
            Q_ASSERT(cur->effectiveRow() == prev->effectiveRow() + 1);
            Q_ASSERT(!prev->d_ptr->m_pModel->rowCount(prev->index()));
        }

        // Check that there it no missing children from the previous
        if ((!prev) || (cur->parent() != prev && cur->parent() != prev->parent() && prev->parent() != prev->d_ptr->m_pRoot)) {
            Q_ASSERT((!prev) || prev->parent()->index().isValid());
//             const int prevParRc = prev->d_ptr->m_pModel->rowCount(prev->parent()->index());
//             Q_ASSERT(prev->effectiveRow() == prevParRc - 1);
        }

        if (vi) {
            //Q_ASSERT(cur->m_Geometry.isValid()); //TODO THIS_COMMIT
            auto geo = cur->m_Geometry.decoratedGeometry();

// //             qDebug() << "BOO" << cur->index().row() << geo.y() << maxX;
            // Prevent accidental overlapping until a view with on-purpose
            // overlapping exists
//             Q_ASSERT(geo.y() >= maxY); // The `=` because it starts at 0
//             Q_ASSERT(cur->m_Geometry.viewTracker()->item()->y() >= maxY); // The `=` because it starts at 0
//             Q_ASSERT(cur->m_Geometry.viewTracker()->item()->y() == cur->m_Geometry.geometry().y());

            // 0x0 elements are generally evil, but this is more about making
            // sure it works at all.
            //Q_ASSERT((geo.y() == 0 && cur->index().row() == 0) || geo.y()); //TODO THIS_COMMIT

            minX = std::min(minX, geo.x());
            minY = std::min(minY, geo.y());
            maxX = std::max(maxX, geo.bottomLeft().x());
            maxY = std::max(maxY, geo.bottomLeft().y());
        }
    } while ((prev = cur) && (cur = TTI(cur->down())));
//     qDebug() << "STOP";

    Q_ASSERT(maxY >= minY || !hadVisible);
    Q_ASSERT(maxX >= minX || !hadVisible);
    //TODO check buffer

    if (firstVisible && !skipVItemState) {
        //Q_ASSERT(prev->m_Geometry.geometry().y() < m_pViewport->currentRect().y());
    }

    if (lastVisible && !skipVItemState) {
        Q_ASSERT(lastVisible->m_Geometry.decoratedGeometry().y() <= m_pViewport->currentRect().bottomLeft().y());
    }

//     Q_ASSERT(maxY < m_pViewport->currentRect().bottomLeft().y() + 100);
//     qDebug() << "SIZE" << maxX << maxY << m_pViewport->currentRect().bottomLeft().y();
}

void TreeTraversalReflectorPrivate::_test_validateViewport(bool skipVItemState)
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    _test_validateLinkedList(skipVItemState);
    int activeCount = 0;

    Q_ASSERT(!((!edges(EdgeType::FREE)->getEdge(Qt::BottomEdge)) ^ (!edges(EdgeType::FREE)->getEdge(Qt::TopEdge))));
    Q_ASSERT(!((!edges(EdgeType::FREE)->getEdge(Qt::LeftEdge)) ^ (!edges(EdgeType::FREE)->getEdge(Qt::RightEdge))));

    if (!edges(EdgeType::FREE)->getEdge(Qt::TopEdge))
        return;

    if (edges(EdgeType::FREE)->getEdge(Qt::TopEdge) == edges(EdgeType::FREE)->getEdge(Qt::BottomEdge)) {
        auto u1 = TTI(edges(EdgeType::FREE)->getEdge(Qt::TopEdge)->up());
        auto d1 = TTI(edges(EdgeType::FREE)->getEdge(Qt::TopEdge)->down());
        auto u2 = TTI(edges(EdgeType::FREE)->getEdge(Qt::BottomEdge)->up());
        auto d2 = TTI(edges(EdgeType::FREE)->getEdge(Qt::BottomEdge)->down());
        Q_ASSERT((!u1) || u1->m_State != StateTracker::ModelItem::State::VISIBLE);
        Q_ASSERT((!u2) || u2->m_State != StateTracker::ModelItem::State::VISIBLE);
        Q_ASSERT((!d1) || d1->m_State != StateTracker::ModelItem::State::VISIBLE);
        Q_ASSERT((!d2) || d2->m_State != StateTracker::ModelItem::State::VISIBLE);
    }

    auto item = TTI(edges(EdgeType::FREE)->getEdge(Qt::TopEdge));
    StateTracker::ModelItem *old = nullptr;

    QRectF oldGeo;

    do {
        Q_ASSERT(old != item);
        Q_ASSERT(item->m_State == StateTracker::ModelItem::State::VISIBLE ||
            (skipVItemState && item->m_State == StateTracker::ModelItem::State::BUFFER)
        );
        if (!skipVItemState) {
            Q_ASSERT(item->m_Geometry.viewTracker());
            Q_ASSERT(item->m_Geometry.viewTracker()->m_State == StateTracker::ViewItem::State::ACTIVE);
        }
        Q_ASSERT(item->up() == old);

        //FIXME don't do this, its temporary so I can add more tests to catch
        // the cause of this failed (::move not updating the position)
//         if (old)
//             item->m_Geometry.m_State.setPosition(QPointF(0.0, oldGeo.y() + oldGeo.height()));

        auto geo =  item->m_Geometry.decoratedGeometry();

        if (geo.width() || geo.height()) {
            Q_ASSERT((!oldGeo.isValid()) || oldGeo.y() < geo.y());
            Q_ASSERT((!oldGeo.isValid()) || oldGeo.y() + oldGeo.height() == geo.y());
        }

//         qDebug() << "TEST" << activeCount << geo;

        activeCount++;
        oldGeo = geo;
    } while ((old = item) && (item = TTI(item->down())));
}

void StateTracker::Index::_test_validate_chain() const
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    auto p = this; //FIXME due to refactor

    int count = 0;

    Q_ASSERT((!p->firstChild()) || p->lastChild());
    Q_ASSERT((!p->firstChild()) || !p->firstChild()->previousSibling());
    Q_ASSERT((!p->lastChild()) || !p->lastChild()->nextSibling());
    auto i = p->firstChild();
    StateTracker::Index *prev = nullptr;
    while(i) {
        Q_ASSERT(i->previousSibling() == prev);
        Q_ASSERT(i->parent() == p);

        //Q_ASSERT((!prev) || i->effectiveRow() == prev->effectiveRow()+1);

        prev = i;
        i = i->nextSibling();
        count++;
    }

    Q_ASSERT(count == loadedChildrenCount());

    if (!prev)
        Q_ASSERT(p->firstChild() == p->lastChild());
    else
        Q_ASSERT(prev == p->lastChild());
}

void TreeTraversalReflectorPrivate::_test_validate_edges()
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    auto vStart = q_ptr->getEdge(IndexMetadata::EdgeType::VISIBLE, Qt::TopEdge);
    auto vEnd   = q_ptr->getEdge(IndexMetadata::EdgeType::VISIBLE, Qt::BottomEdge);

    Q_ASSERT((!vStart) || (vStart && vEnd));

    if (vStart) {
        Q_ASSERT(TTI(vStart->modelTracker())->m_State == StateTracker::ModelItem::State::VISIBLE);
        Q_ASSERT(TTI(vEnd->modelTracker())->m_State == StateTracker::ModelItem::State::VISIBLE);
        auto prev = TTI(vStart->modelTracker()->up());
        auto next = TTI(vEnd->modelTracker()->down());

        Q_ASSERT((!prev) || prev->m_State == StateTracker::ModelItem::State::REACHABLE);
        Q_ASSERT((!next) || next->m_State == StateTracker::ModelItem::State::REACHABLE);
    }
}

void TreeTraversalReflectorPrivate::_test_validate_move(StateTracker::Index* parentTTI,
                                                        StateTracker::Index* startTTI,
                                                        StateTracker::Index* endTTI,
                                                        StateTracker::Index* newPrevTTI,
                                                        StateTracker::Index* newNextTTI,
                                                        int row)
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    Q_ASSERT((newPrevTTI || startTTI) && newPrevTTI != startTTI);
    Q_ASSERT((newNextTTI || endTTI  ) && newNextTTI != endTTI  );

    // Update the tree parent (if necessary)
    Q_ASSERT(startTTI->parent() == parentTTI);
    Q_ASSERT(endTTI->parent()   == parentTTI);

    //BEGIN debug
    if (newPrevTTI && newPrevTTI->parent())
        newPrevTTI->parent()->_test_validate_chain();
    if (startTTI && startTTI->parent())
        startTTI->parent()->_test_validate_chain();
    if (endTTI && endTTI->parent())
        endTTI->parent()->_test_validate_chain();
    if (newNextTTI && newNextTTI->parent())
        newNextTTI->parent()->_test_validate_chain();
    //END debug

    if (endTTI->nextSibling()) {
        Q_ASSERT(endTTI->nextSibling()->previousSibling() ==endTTI);
    }

    if (startTTI->previousSibling()) {
        Q_ASSERT(startTTI->previousSibling()->parent() == startTTI->parent());
        Q_ASSERT(startTTI->previousSibling()->nextSibling() ==startTTI);
    }

    Q_ASSERT(parentTTI->firstChild());
    Q_ASSERT(row || parentTTI->firstChild() == startTTI);
}

void TreeTraversalReflectorPrivate::_test_validate_edges_simple()
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    //BEGIN test
    auto tve = edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);

    Q_ASSERT((!tve) || (TTI(tve)->m_State == StateTracker::ModelItem::State::VISIBLE));
    Q_ASSERT((!tve) || (!tve->up()) || (TTI(tve->up())->m_State != StateTracker::ModelItem::State::VISIBLE));

    auto bve = edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);

    Q_ASSERT((!bve) || (TTI(bve)->m_State == StateTracker::ModelItem::State::VISIBLE));
    Q_ASSERT((!bve) || (!bve->down()) || (TTI(bve->down())->m_State != StateTracker::ModelItem::State::VISIBLE));
    //END test
}

void TreeTraversalReflectorPrivate::_test_validate_geometry_cache()
{
    auto bve = edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);
    for (auto i = TTI(m_pRoot->firstChild()); i; i = TTI(i->down())) {
        Q_ASSERT(i->m_Geometry.removeMe() == (int)StateTracker::Geometry::State::VALID);

        if (i == bve)
            return;
    }
}

void TreeTraversalReflectorPrivate::_test_print_state()
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    auto item = TTI(m_pRoot->firstChild());

    if (!item)
        return;

    int i = 0;

    do {
        qDebug() << i++
            << item
            << item->depth()
            << item->m_Geometry.isInSync()
            << (item->m_State == StateTracker::ModelItem::State::VISIBLE);
    } while((item = TTI(item->down())));
}

void TreeTraversalReflectorPrivate::_test_validateUnloaded(const QModelIndex& parent, int first, int last)
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    Q_ASSERT(m_pModel);
    for (int i = first; i <= last; i++) {
        const auto idx = m_pModel->index(i, 0, parent);
        Q_ASSERT(idx.isValid());
        Q_ASSERT(!ttiForIndex(idx));
    }
}

// Validate that there is no holes in the view.
void TreeTraversalReflectorPrivate::_test_validateContinuity()
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    auto item = edges(EdgeType::VISIBLE)->getEdge(Qt::TopEdge);
    auto bve = edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);

    if (!item)
        return;

    auto idx = item->index();

    do {
        const auto oldIdx = idx;
        volatile auto oldItem = item;
        Q_ASSERT(item->index() == idx);
        Q_ASSERT(oldIdx != (idx = getNextIndex(idx)));
        Q_ASSERT(oldItem != (item = item->down()));
    } while(item && item != bve);
}

void TreeTraversalReflectorPrivate::_test_validateAtEnd()
{
#ifndef ENABLE_EXTRA_VALIDATION
    return;
#endif
    _test_validateContinuity();

    auto bve = edges(EdgeType::VISIBLE)->getEdge(Qt::BottomEdge);
    Q_ASSERT((!m_pRoot->firstChild()) || bve); //TODO wrong

    if (!bve)
        return;

    const auto next = getNextIndex(bve->index());

    Q_ASSERT(!next.isValid());
}
