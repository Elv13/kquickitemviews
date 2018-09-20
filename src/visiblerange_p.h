/***************************************************************************
 *   Copyright (C) 2018 by Emmanuel Lepage Vallee                          *
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


/**
 * For size modes like UniformRowHeight, it's pointless to keep track of
 * every single elements (potentially without a TreeTraversalItem).
 *
 * Even in the case of individual item, it may not be worth the extra memory
 * and recomputing them on demand may make sense.
 */
struct BlockMetadata
{
    QPointF m_Position;
    QSizeF  m_Size;
};

/**
 * In order to keep the separation of concerns design goal intact, this
 * interface between the TreeTraversalReflector and VisibleRange internal
 * metadata without exposing them.
 */
class VisibleRangeSync final
{
public:
    inline void updateSingleItem(const QModelIndex& index, BlockMetadata* b);
};