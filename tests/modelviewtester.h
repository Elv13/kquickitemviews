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

#include <QAbstractItemModel>
#include <QTimer>

struct ModelViewTesterItem;

/**
 * Perform every model operations so the all the Q_ASSERT and extra validation
 * code in the TreeView2 can be used.
 *
 * It could not be a QStandardItemModel because it lacks fine grained control
 * over the move operations.
 */
class ModelViewTester : public QAbstractItemModel
{
Q_OBJECT

public:
    Q_PROPERTY(int interval READ interval WRITE setInterval)


    explicit ModelViewTester(QObject* parent = nullptr);
    virtual ~ModelViewTester();

    Q_INVOKABLE void run();

    int interval() const {return m_pTimer->interval(); }
    void setInterval(int i) { m_pTimer->setInterval(i); }

    //Model implementation
    virtual bool          setData      ( const QModelIndex& index, const QVariant &value, int role   ) override;
    virtual QVariant      data         ( const QModelIndex& index, int role = Qt::DisplayRole        ) const override;
    virtual int           rowCount     ( const QModelIndex& parent = QModelIndex()                   ) const override;
//     virtual Qt::ItemFlags flags        ( const QModelIndex& index                                    ) const override;
    virtual int           columnCount  ( const QModelIndex& parent = QModelIndex()                   ) const override;
    virtual QModelIndex   parent       ( const QModelIndex& index                                    ) const override;
    virtual QModelIndex   index        ( int row, int column, const QModelIndex& parent=QModelIndex()) const override;
    virtual QMimeData*    mimeData     ( const QModelIndexList &indexes                              ) const override;
    virtual bool          dropMimeData ( const QMimeData* data, Qt::DropAction action, int row, int column, const QModelIndex& parent ) override;
    virtual QHash<int,QByteArray> roleNames() const override;

public Q_SLOTS:
    //Test function
    void prependSimpleRoot();
    void appendSimpleRoot();
    void appendRootChildren();

    void moveRootToFront();
    void moveChildByOne();
    void moveChildByParent();
    void moveToGrandChildren();

    void insertRoot();
    void insertFirst();
    void insertChild();

    void removeRoot();
    void resetModel();

    void largeFrontTree();
    void removeLargeTree();
    void removeLargeTree2();
    void largeFrontTree2();
    void removeLargeTree3();

private:
    ModelViewTesterItem* m_pRoot;

    int count {0};
    QStringList steps;
    QTimer *m_pTimer {new QTimer(this)};
};
