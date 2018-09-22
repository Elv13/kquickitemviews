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
#include "plugin.h"

#include <QtCore/QDebug>

#include "adapters/decorationadapter.h"
#include "adapters/scrollbaradapter.h"
#include "views/hierarchyview.h"
#include "views/listview.h"
#include "views/treeview.h"
#include "views/comboboxview.h"
#include "flickablescrollbar.h"
#include "proxies/sizehintproxymodel.h"

void KQuickView::registerTypes(const char *uri)
{
    Q_ASSERT(uri == QLatin1String("org.kde.playground.kquickview"));

    qmlRegisterType<HierarchyView>(uri, 1, 0, "HierarchyView");
    qmlRegisterType<TreeView>(uri, 1, 0, "TreeView");
    qmlRegisterType<ListView>(uri, 1, 0, "ListView");
    qmlRegisterType<ScrollBarAdapter>(uri, 1, 0, "ScrollBarAdapter");
    qmlRegisterType<DecorationAdapter>(uri, 1,0, "DecorationAdapter");
    qmlRegisterType<ComboBoxView>(uri, 1, 0, "ComboBoxView");
    qmlRegisterType<FlickableScrollBar>(uri, 1, 0, "FlickableScrollBar");
    qmlRegisterType<SizeHintProxyModel>(uri, 1, 0, "SizeHintProxyModel");
    qmlRegisterUncreatableType<ListViewSections>(uri, 1, 0, "ListViewSections", "");
}

void KQuickView::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(engine)
    Q_UNUSED(uri)
}
