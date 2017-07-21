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
#pragma once

#include <QQuickItem>
#include <QtCore/QItemSelectionModel>

class BindedComboBoxPrivate;

/**
 * Extended QtQuickControls2 ComboBox with proper selection model support.
 */
class BindedComboBox : public QQuickItem
{
    Q_OBJECT
public:
    Q_PROPERTY(QItemSelectionModel* selectionModel READ selectionModel WRITE setSelectionModel)

    explicit BindedComboBox(QQuickItem* parent = nullptr);
    virtual ~BindedComboBox();

    QItemSelectionModel* selectionModel() const;
    void setSelectionModel(QItemSelectionModel* s);

private:
    BindedComboBoxPrivate* d_ptr;
    Q_DECLARE_PRIVATE(BindedComboBox)
};
// Q_DECLARE_METATYPE(BindedComboBox*)
