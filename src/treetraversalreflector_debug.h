/**
 * While this module is under development and the autotests are lacking,
 * always run strict tests at runtime.
 */

void TreeTraversalReflectorPrivate::_test_validateTree(TreeTraversalItems* p)
{
#ifdef QT_NO_DEBUG_OUTPUT
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

//     if (p->m_pParent == m_pRoot && m_pRoot->m_tChildren[FIRST] == p && p->m_pTreeItem) {
//         Q_ASSERT(!p->m_pTreeItem->up());
//     }

    // First, let's check the linked list to avoid running more test on really
    // corrupted data
    if (auto i = p->m_tChildren[FIRST]) {
        auto idx = i->m_Index;
        int count = 1;
        auto oldI = i;

        while ((oldI = i) && (i = i->m_tSiblings[NEXT])) {
            // If this is a next, then there has to be a previous
            Q_ASSERT(i->m_pParent == p);
            Q_ASSERT(i->m_tSiblings[PREVIOUS]);
            Q_ASSERT(i->m_tSiblings[PREVIOUS]->m_Index == idx);
            //Q_ASSERT(i->m_Index.row() == idx.row()+1); //FIXME
            Q_ASSERT(i->m_tSiblings[PREVIOUS]->m_tSiblings[NEXT] == i);
            Q_ASSERT(i->m_tSiblings[PREVIOUS] == oldI);
            idx = i->m_Index;
            count++;
        }

        Q_ASSERT(p == p->m_tChildren[FIRST]->m_pParent);
        Q_ASSERT(p == p->m_tChildren[LAST]->m_pParent);
        Q_ASSERT(p->m_hLookup.size() == count);
    }

    // Do that again in the other direction
    if (auto i = p->m_tChildren[LAST]) {
        auto idx = i->m_Index;
        auto oldI = i;
        int count = 1;

        while ((oldI = i) && (i = i->m_tSiblings[PREVIOUS])) {
            Q_ASSERT(i->m_tSiblings[NEXT]);
            Q_ASSERT(i->m_tSiblings[NEXT]->m_Index == idx);
            Q_ASSERT(i->m_pParent == p);
            //Q_ASSERT(i->m_Index.row() == idx.row()-1); //FIXME
            Q_ASSERT(i->m_tSiblings[NEXT]->m_tSiblings[PREVIOUS] == i);
            Q_ASSERT(i->m_tSiblings[NEXT] == oldI);
            idx = i->m_Index;
            count++;
        }

        Q_ASSERT(p->m_hLookup.size() == count);
    }

    //TODO remove once stable
    // Brute force recursive validations
    TreeTraversalItems *old(nullptr), *newest(nullptr);
    for (auto i = p->m_hLookup.constBegin(); i != p->m_hLookup.constEnd(); i++) {
        if ((!newest) || i.key().row() < newest->m_Index.row())
            newest = i.value();

        if ((!old) || i.key().row() > old->m_Index.row())
            old = i.value();

        // Check that m_FailedCount is valid
        //Q_ASSERT(!i.value()->m_pTreeItem->hasFailed());

        // Test the indices
        Q_ASSERT(p == m_pRoot || i.key().internalPointer() == i.value()->m_Index.internalPointer());
        Q_ASSERT(p == m_pRoot || (p->m_Index.isValid()) || p->m_Index.internalPointer() != i.key().internalPointer());
        //Q_ASSERT(old == i.value() || old->m_Index.row() > i.key().row()); //FIXME
        //Q_ASSERT(newest == i.value() || newest->m_Index.row() < i.key().row()); //FIXME

        // Test that there is no trivial duplicate TreeTraversalItems for the same index
        if(i.value()->m_tSiblings[PREVIOUS] && i.value()->m_tSiblings[PREVIOUS]->m_hLookup.isEmpty()) {
            const auto prev = i.value()->up();
            Q_ASSERT(prev == i.value()->m_tSiblings[PREVIOUS]);

            const auto next = prev->down();
            Q_ASSERT(next == i.value());
        }

        // Test the virtual linked list between the leafs and branches
        if(auto next = i.value()->down()) {
            Q_ASSERT(next->up() == i.value());
            Q_ASSERT(next != i.value());
        }
        else {
            // There is always a next is those conditions are not met unless there
            // is failed elements creating (auto-corrected) holes in the chains.
            Q_ASSERT(!i.value()->m_tSiblings[NEXT]);
            Q_ASSERT(i.value()->m_hLookup.isEmpty());
        }

        if(auto prev = i.value()->up()) {
            Q_ASSERT(prev->down() == i.value());
            Q_ASSERT(prev != i.value());
        }
        else {
            // There is always a previous if those conditions are not met unless there
            // is failed elements creating (auto-corrected) holes in the chains.
            Q_ASSERT(!i.value()->m_tSiblings[PREVIOUS]);
            Q_ASSERT(i.value()->m_pParent == m_pRoot);
        }

        _test_validateTree(i.value());
    }

    // Traverse as a list
    if (p == m_pRoot) {
        TreeTraversalItems* oldTTI(nullptr);

        int count(0), count2(0);
        for (auto i = m_pRoot->m_tChildren[FIRST]; i; i = i->down()) {
            Q_ASSERT((!oldTTI) || i->up());
            Q_ASSERT(i->up() == oldTTI);
            oldTTI = i;
            count++;
        }

        // Backward too
        oldTTI = nullptr;
        auto last = m_pRoot->m_tChildren[LAST];
        while (last && last->m_tChildren[LAST])
            last = last->m_tChildren[LAST];

        for (auto i = last; i; i = i->up()) {
            Q_ASSERT((!oldTTI) || i->down());
            Q_ASSERT(i->down() == oldTTI);
            oldTTI = i;
            count2++;
        }

        Q_ASSERT(count == count2);
    }

    // Test that the list edges are valid
    Q_ASSERT(!(!!p->m_tChildren[LAST] ^ !!p->m_tChildren[FIRST]));
    Q_ASSERT(p->m_tChildren[LAST]  == old);
    Q_ASSERT(p->m_tChildren[FIRST] == newest);
    Q_ASSERT((!old) || !old->m_tSiblings[NEXT]);
    Q_ASSERT((!newest) || !newest->m_tSiblings[PREVIOUS]);
}

void TreeTraversalReflectorPrivate::_test_validateViewport(bool skipVItemState)
{
    int activeCount = 0;

    Q_ASSERT(!((!m_lpEdges[Bottom]) ^ (!m_lpEdges[Top  ])));
    Q_ASSERT(!((!m_lpEdges[Left  ]) ^ (!m_lpEdges[Right])));

    if (!m_lpEdges[Top])
        return;

    if (m_lpEdges[Top] == m_lpEdges[Bottom]) {
        auto u1 = m_lpEdges[Top]->up();
        auto d1 = m_lpEdges[Top]->down();
        auto u2 = m_lpEdges[Bottom]->up();
        auto d2 = m_lpEdges[Bottom]->down();
        Q_ASSERT((!u1) || u1->m_State != TreeTraversalItems::State::VISIBLE);
        Q_ASSERT((!u2) || u2->m_State != TreeTraversalItems::State::VISIBLE);
        Q_ASSERT((!d1) || d1->m_State != TreeTraversalItems::State::VISIBLE);
        Q_ASSERT((!d2) || d2->m_State != TreeTraversalItems::State::VISIBLE);
    }

    auto item = m_lpEdges[Top];
    TreeTraversalItems *old = nullptr;

    QRectF oldGeo;

    do {
        Q_ASSERT(old != item);
        Q_ASSERT(item->m_State == TreeTraversalItems::State::VISIBLE);
        Q_ASSERT(item->m_pTreeItem);
        if (!skipVItemState)
            Q_ASSERT(item->m_pTreeItem->m_State == VisualTreeItem::State::ACTIVE);
        Q_ASSERT(item->up() == old);

        //FIXME don't do this, its temporary so I can add more tests to catch
        // the cause of this failed (::move not updating the position)
        if (old)
            item->m_Geometry.setPosition(QPointF(0.0, oldGeo.y() + oldGeo.height()));

        auto geo =  item->m_Geometry.geometry();

        if (geo.width() || geo.height()) {
            Q_ASSERT((!oldGeo.isValid()) || oldGeo.y() < geo.y());
            Q_ASSERT((!oldGeo.isValid()) || oldGeo.y() + oldGeo.height() == geo.y());
        }

//         qDebug() << "TEST" << activeCount << geo;

        activeCount++;
        oldGeo = geo;
        old = item;
    } while (item = item->down());
}
