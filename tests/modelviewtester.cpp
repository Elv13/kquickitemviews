/***************************************************************************
 *   Copyright (C) 2017 by Emmanuel Lepage Vallee                          *
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
#include "modelviewtester.h"

#include <QtCore/QDebug>
#include <QMetaObject>
#include <QMetaMethod>

#include <functional>

#define DO(slot) steps << QString(#slot) ;

struct ModelViewTesterItem
{
    ModelViewTesterItem() {}
    ModelViewTesterItem(ModelViewTesterItem* p, const QHash<int, QVariant>& vals, int i = -1);

    int m_Index {0};;
    ModelViewTesterItem* m_pParent {nullptr};
    QHash<int, QVariant> m_hValues;
    QVector<ModelViewTesterItem*> m_lChildren;
};

ModelViewTester::ModelViewTester(QObject* parent) : QAbstractItemModel(parent)
{
    m_pRoot = new ModelViewTesterItem;

    // Append
    DO(appendSimpleRoot);
    DO(appendSimpleRoot);
    DO(appendSimpleRoot);
    DO(appendSimpleRoot);
    DO(appendSimpleRoot);

    DO(appendRootChildren);
    DO(appendRootChildren);
    DO(appendRootChildren);
    DO(appendRootChildren);

    // Prepend
    DO(prependSimpleRoot);

    // Move
    DO(moveRootToFront);
    DO(moveChildByOne);
    DO(moveChildByParent);
    DO(moveToGrandChildren);
    //TODO moveFirst
    //TODO moveLast

    // Insert
    DO(insertRoot);
    DO(insertFirst);
    DO(insertChild);

    // Remove
    DO(removeRoot);
    //TODO removeMiddle
    //TODO removeLastChild
    //TODO removeWithChildren
    DO(resetModel);

    // Larger tree
    DO(largeFrontTree);
    DO(removeLargeTree);
    DO(removeLargeTree2);
    DO(largeFrontTree2);
    DO(removeLargeTree2);
    DO(removeLargeTree3);
    //TODO move multiple

    // Larger move (with out of view)

}

ModelViewTester::~ModelViewTester()
{

}

void ModelViewTester::run() {
    m_pTimer->setInterval(100);

    QObject::connect(m_pTimer, &QTimer::timeout, this, [this]() {
        int methodIndex = metaObject()->indexOfMethod((steps[count]+"()").toLatin1());
        metaObject()->method(methodIndex).invoke(this, Qt::QueuedConnection);
        count++;
        if (count == steps.size()) {
            m_pTimer->stop();
            count = 0;
        }
    });

    m_pTimer->start();
}

bool ModelViewTester::setData( const QModelIndex& index, const QVariant &value, int role   )
{
    Q_UNUSED(index)
    Q_UNUSED(value)
    Q_UNUSED(role)
    return false;
}

QVariant ModelViewTester::data( const QModelIndex& index, int role ) const
{
    if (!index.isValid())
        return {};

    auto item = static_cast<ModelViewTesterItem*>(index.internalPointer());

    return item->m_hValues[role];
}

int ModelViewTester::rowCount( const QModelIndex& parent) const
{
    if (!parent.isValid())
        return m_pRoot->m_lChildren.size();

    auto item = static_cast<ModelViewTesterItem*>(parent.internalPointer());

    return item->m_lChildren.size();
}

int ModelViewTester::columnCount( const QModelIndex& parent ) const
{
    return parent.isValid() ? 0 : 1; //FIXME not really true
}

QModelIndex ModelViewTester::parent( const QModelIndex& index ) const
{
    if (!index.isValid())
        return {};

    auto item = static_cast<ModelViewTesterItem*>(index.internalPointer());

    Q_ASSERT(item != m_pRoot);

    if (item->m_pParent == m_pRoot)
        return {};

    return createIndex(item->m_pParent->m_Index, 0, item->m_pParent);
}

QModelIndex ModelViewTester::index( int row, int column, const QModelIndex& parent ) const
{
    auto parItem = parent.isValid() ?
        static_cast<ModelViewTesterItem*>(parent.internalPointer()): m_pRoot;

    if (column || row >= parItem->m_lChildren.size() || row < 0)
        return {};

    return createIndex(row, column, parItem->m_lChildren[row]);
}

QMimeData* ModelViewTester::mimeData( const QModelIndexList &indexes) const
{
    Q_UNUSED(indexes)
    return nullptr; //TODO
}

bool ModelViewTester::dropMimeData( const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent)
{
    Q_UNUSED(data)
    Q_UNUSED(action)
    Q_UNUSED(row)
    Q_UNUSED(column)
    Q_UNUSED(parent)
    return false; //TODO
}

QHash<int,QByteArray> ModelViewTester::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {Qt::UserRole, "offset"}
    };
}

// Qt::ItemFlags ModelViewTester::flags( const QModelIndex& index) const

ModelViewTesterItem::ModelViewTesterItem(ModelViewTesterItem* p, const QHash<int, QVariant>& vals, int i) :
    m_pParent(p), m_hValues(vals)
{
    if (i == -1) {
        m_Index = p->m_lChildren.size();
        p->m_lChildren << this;
    }
    else {
        m_Index = i;

        p->m_lChildren.insert(i, this);
        for (int j=i+1; j < p->m_lChildren.size(); j++)
            p->m_lChildren[j]->m_Index++;
    }
}

void ModelViewTester::prependSimpleRoot()
{
    beginInsertRows({}, 0, 0);

    QHash<int, QVariant> vals = {
        {Qt::DisplayRole, "prep root 1"},
        {Qt::UserRole, 0}
    };

    new ModelViewTesterItem(m_pRoot, vals, 0);

    endInsertRows();
    beginInsertRows({}, 1, 1);

    vals = {
        {Qt::DisplayRole, "prep root 2"},
        {Qt::UserRole, 0}
    };

    new ModelViewTesterItem(m_pRoot, vals, 1);

    endInsertRows();
    beginInsertRows({}, 0, 0);

    vals = {
        {Qt::DisplayRole, "prep root 0"},
        {Qt::UserRole, 0}
    };

    new ModelViewTesterItem(m_pRoot, vals, 0);

    endInsertRows();
}

void ModelViewTester::appendSimpleRoot()
{
    beginInsertRows({}, m_pRoot->m_lChildren.size(), m_pRoot->m_lChildren.size());

    QHash<int, QVariant> vals = {
        {Qt::DisplayRole, "root "+QString::number(m_pRoot->m_lChildren.size())},
        {Qt::UserRole, 0}
    };

    new ModelViewTesterItem(m_pRoot, vals);

    endInsertRows();
}

void ModelViewTester::appendRootChildren()
{
    auto par = m_pRoot->m_lChildren[1];
    beginInsertRows(index(1,0), par->m_lChildren.size(), par->m_lChildren.size());

    QHash<int, QVariant> vals = {
        {Qt::DisplayRole, "child "+QString::number(par->m_lChildren.size())},
        {Qt::UserRole, 10}
    };

    new ModelViewTesterItem(par, vals);

    endInsertRows();
}

void ModelViewTester::moveRootToFront()
{
    beginMoveRows({}, 2,2, {}, 0);

    auto elem = m_pRoot->m_lChildren[2];

    m_pRoot->m_lChildren.remove(2);
    m_pRoot->m_lChildren.insert(0, elem);

    for (int i =0; i < m_pRoot->m_lChildren.size(); i++)
        m_pRoot->m_lChildren[i]->m_Index = i;

    endMoveRows();
}

void ModelViewTester::moveChildByOne()
{
    auto parentIdx = index(4,0);

    beginMoveRows(parentIdx, 2,2, parentIdx, 1);

    auto elem = m_pRoot->m_lChildren[4]->m_lChildren[2];

    m_pRoot->m_lChildren[4]->m_lChildren.remove(2);
    m_pRoot->m_lChildren[4]->m_lChildren.insert(0, elem);

    for (int i =0; i < m_pRoot->m_lChildren[4]->m_lChildren.size(); i++)
        m_pRoot->m_lChildren[4]->m_lChildren[i]->m_Index = i;

    endMoveRows();
}

void ModelViewTester::moveChildByParent()
{
    auto elem = m_pRoot->m_lChildren[4]->m_lChildren[3];

    auto oldParentIdx = index(4,0);
    auto newParentIdx = index(m_pRoot->m_lChildren.size()-1,0);

    beginMoveRows(oldParentIdx, 3,3, newParentIdx, 0);

    m_pRoot->m_lChildren[4]->m_lChildren.remove(3);
    m_pRoot->m_lChildren[m_pRoot->m_lChildren.size()-1]->m_lChildren.insert(0, elem);

    elem->m_Index = 0;
    endMoveRows();
}

void ModelViewTester::moveToGrandChildren()
{
    auto elem1 = m_pRoot->m_lChildren[1];
    auto elem2 = m_pRoot->m_lChildren[2];
    auto newPar = m_pRoot->m_lChildren[4]->m_lChildren[2];
    auto newParentIdx = createIndex(newPar->m_Index, 0, newPar);

    beginMoveRows({}, 1,2, newParentIdx, 0);

    elem1->m_pParent = newPar;
    elem2->m_pParent = newPar;

    elem1->m_hValues = {
        {Qt::DisplayRole, elem1->m_hValues[0].toString()+" gc"},
        {Qt::UserRole, 20}
    };
    elem2->m_hValues = {
        {Qt::DisplayRole, elem2->m_hValues[0].toString()+" gc"},
        {Qt::UserRole, 20}
    };

    m_pRoot->m_lChildren.remove(1);
    m_pRoot->m_lChildren.remove(1);

    newPar->m_lChildren << elem1 << elem2;

    for (int i =0; i < newPar->m_lChildren.size(); i++)
        newPar->m_lChildren[i]->m_Index = i;

    for (int i =0; i < m_pRoot->m_lChildren.size(); i++)
        m_pRoot->m_lChildren[i]->m_Index = i;

    endMoveRows();

    Q_EMIT dataChanged(index(0, 0, newParentIdx), index(1, 0, newParentIdx));
}


void ModelViewTester::insertRoot()
{
    beginInsertRows({}, 1, 1);

    QHash<int, QVariant> vals = {
        {Qt::DisplayRole, "inserted root 1"},
        {Qt::UserRole, 0}
    };

    new ModelViewTesterItem(m_pRoot, vals, 1);

    endInsertRows();
}

void ModelViewTester::insertFirst()
{
    beginInsertRows({}, 0, 0);

    QHash<int, QVariant> vals = {
        {Qt::DisplayRole, "inserted root 0"},
        {Qt::UserRole, 0}
    };

    new ModelViewTesterItem(m_pRoot, vals, 0);

    endInsertRows();
}

void ModelViewTester::insertChild()
{
    auto newPar = m_pRoot->m_lChildren[4];
    auto parIdx = createIndex(4, 0, newPar);

    Q_ASSERT(parIdx.isValid());

    beginInsertRows(parIdx, 0, 0);

    QHash<int, QVariant> vals = {
        {Qt::DisplayRole, "inserted child 0"},
        {Qt::UserRole, 0}
    };

    new ModelViewTesterItem(newPar, vals, 0);

    endInsertRows();
}

void ModelViewTester::removeRoot()
{
    beginRemoveRows({}, 0, 0);

    QHash<int, QVariant> vals = {
        {Qt::DisplayRole, "inserted root 0"},
        {Qt::UserRole, 0}
    };

    m_pRoot->m_lChildren.remove(1);

    for (int i =0; i < m_pRoot->m_lChildren.size(); i++)
        m_pRoot->m_lChildren[i]->m_Index = i;

    endRemoveRows();
}

void ModelViewTester::resetModel()
{
    beginResetModel();
    qDeleteAll(m_pRoot->m_lChildren);
    m_pRoot->m_lChildren.clear();
    endResetModel();
}

void ModelViewTester::largeFrontTree()
{
    for (int i = 0; i < 100; i++) {
        beginInsertRows({}, 0, 0);

        QHash<int, QVariant> vals = {
            {Qt::DisplayRole, "inserted root 1"},
            {Qt::UserRole, 0}
        };

        auto itm = new ModelViewTesterItem(m_pRoot, vals, 0);

        endInsertRows();

        auto p = createIndex(0, 0, itm);

        beginInsertRows(p, 0, 4);
        for (int j = 0; j < 5; j++) {
            QHash<int, QVariant> vals2 = {
                {Qt::DisplayRole, "children "+QString::number(j)},
                {Qt::UserRole, 0}
            };

            new ModelViewTesterItem(itm, vals2, 0);
        }
        endInsertRows();
    }
}

// Test removing elements when some are out of view
void ModelViewTester::removeLargeTree()
{
    for (int i = 0; i < 100; i++) {
        auto parent = m_pRoot->m_lChildren[i];
        auto idx = createIndex(i, 0, parent);

        beginRemoveRows(idx, 3, 3);
        delete parent->m_lChildren[3];
        parent->m_lChildren.remove(3);
        endRemoveRows();
    }
}

// Test removing multiple item at once with out-of-view
void ModelViewTester::removeLargeTree2()
{
    for (int i = 0; i < 100; i++) {
        auto parent = m_pRoot->m_lChildren[i];
        const int s =  parent->m_lChildren.size();
        auto idx = createIndex(i, 0, parent);

        beginRemoveRows(idx, 0, s);
        for (int j = 0; j < s; j++)
            delete parent->m_lChildren[j];
        parent->m_lChildren.clear();
        endRemoveRows();
    }
}

// Test removing out-of-view item until the viewport is empty
void ModelViewTester::removeLargeTree3()
{
    while (m_pRoot->m_lChildren.size()) {
        const int pos = m_pRoot->m_lChildren.size()/2;
        beginRemoveRows({}, pos, pos);
        delete m_pRoot->m_lChildren[pos];
        m_pRoot->m_lChildren.remove(pos);
        endRemoveRows();
    }
}

// Insert more items that can fit in the view
void ModelViewTester::largeFrontTree2()
{
    for (int i = 0; i < 100; i++) {
        auto parent = m_pRoot->m_lChildren[i];
        auto idx = createIndex(i, 0, parent);

        beginInsertRows(idx, 0, 19);
        for (int j = 0; j < 20; j++) {
            QHash<int, QVariant> vals = {
                {Qt::DisplayRole, "children v2 "+QString::number(j)},
                {Qt::UserRole, 0}
            };

            new ModelViewTesterItem(parent, vals, 0);
        }
        endInsertRows();
    }
}
