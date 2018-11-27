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
#include "geometry_p.h"

#include <QtCore/QDebug>

#include <private/viewport_p.h>

#define S StateTracker::Geometry::State::
const StateTracker::Geometry::State StateTracker::Geometry::m_fStateMap[5][7] = {
/*              MOVE      RESIZE      PLACE     RESET    MODIFY     DECORATE       VIEW   */
/*INIT     */ { S INIT, S SIZE   , S POSITION, S INIT, S INIT    , S INIT    , S INIT     },
/*SIZE     */ { S SIZE, S SIZE   , S PENDING , S INIT, S SIZE    , S SIZE    , S SIZE     },
/*POSITION */ { S INIT, S PENDING, S POSITION, S INIT, S POSITION, S POSITION, S POSITION },
/*PENDING  */ { S SIZE, S PENDING, S PENDING , S INIT, S POSITION, S PENDING , S VALID    },
/*VALID    */ { S SIZE, S PENDING, S PENDING , S INIT, S POSITION, S PENDING , S VALID    },
};
#undef S

#define A &StateTracker::Geometry::
const StateTracker::Geometry::StateF StateTracker::Geometry::m_fStateMachine[5][7] = {
/*                  MOVE        RESIZE        PLACE         RESET         MODIFY      DECORATE       VIEW    */
/*INIT     */ { A nothing   , A nothing  , A nothing  , A nothing   , A nothing   , A nothing  , A error     },
/*SIZE     */ { A nothing   , A dropSize , A nothing  , A dropSize  , A nothing   , A nothing  , A error     },
/*POSITION */ { A invalidate, A nothing  , A nothing  , A dropPos   , A nothing   , A nothing  , A error     },
/*PENDING  */ { A nothing   , A nothing  , A nothing  , A invalidate, A invalidate, A nothing  , A buildCache},
/*VALID    */ { A nothing   , A dropCache, A dropCache, A invalidate, A dropCache , A dropCache, A buildCache},
};
#undef A

void StateTracker::Geometry::nothing()
{}

void StateTracker::Geometry::invalidate()
{
    dropCache();
    qDebug() << "INVALIDATE" << m_Position << m_Size;
}

void StateTracker::Geometry::error()
{
    Q_ASSERT(false);
}

void StateTracker::Geometry::dropCache()
{
    qDebug() << "==DROP CACHE";
    //TODO
}

void StateTracker::Geometry::buildCache()
{
    //TODO
}

void StateTracker::Geometry::dropSize()
{
    dropCache();
    m_Size = {};
}

void StateTracker::Geometry::dropPos()
{
    dropCache();
    m_Position = {};
}

StateTracker::Geometry::State StateTracker::Geometry::performAction(IndexMetadata::GeometryAction a)
{
    const int s = (int)m_State;
    m_State     = m_fStateMap[s][(int)a];

    (this->*StateTracker::Geometry::m_fStateMachine[s][(int)a])();

    // Validate the state
    //BEGIN debug
    switch(m_State) {
        case StateTracker::Geometry::State::VALID:
        case StateTracker::Geometry::State::PENDING:
            Q_ASSERT(!(m_Position.x() == -1 && m_Position.y() == -1));
            Q_ASSERT(m_Size.isValid());
            break;
        case StateTracker::Geometry::State::SIZE:
            Q_ASSERT(m_Size.isValid());
            break;
        case StateTracker::Geometry::State::POSITION:
            Q_ASSERT(!(m_Position.x() == -1 && m_Position.y() == -1));
            break;
        case StateTracker::Geometry::State::INIT:
            break;
    }
    //END debug

    return m_State;
}

QRectF StateTracker::Geometry::rawGeometry() const
{
    const_cast<StateTracker::Geometry*>(this)->performAction(IndexMetadata::GeometryAction::VIEW);

    Q_ASSERT(m_State == StateTracker::Geometry::State::VALID);

    return QRectF(m_Position, m_Size);
}

QRectF StateTracker::Geometry::contentGeometry() const
{
    auto g = rawGeometry();

    g.setY(g.y() + borderDecoration( Qt::TopEdge  ));
    g.setX(g.x() + borderDecoration( Qt::LeftEdge ));

    return g;
}

QRectF StateTracker::Geometry::decoratedGeometry() const
{
    //TODO cache this, extend the state machine to have a "really valid" state
    auto g = rawGeometry();

    const auto topDeco = borderDecoration( Qt::TopEdge    );
    const auto botDeco = borderDecoration( Qt::BottomEdge );
    const auto lefDeco = borderDecoration( Qt::LeftEdge   );
    const auto rigDeco = borderDecoration( Qt::RightEdge  );

    g.setHeight(g.height() + topDeco + botDeco);
    g.setWidth (g.width () + lefDeco + rigDeco);

    return g;
}

QSizeF StateTracker::Geometry::size() const
{
    Q_ASSERT(m_State != StateTracker::Geometry::State::INIT);
    Q_ASSERT(m_State != StateTracker::Geometry::State::POSITION);

    return m_State == StateTracker::Geometry::State::SIZE ?
        m_Size : decoratedGeometry().size();
}

QPointF StateTracker::Geometry::position() const
{
    return m_State == StateTracker::Geometry::State::POSITION ?
        m_Position : decoratedGeometry().topLeft();
}

void StateTracker::Geometry::setPosition(const QPointF& pos)
{
    m_Position = pos;
    performAction(IndexMetadata::GeometryAction::PLACE);
}

void StateTracker::Geometry::setSize(const QSizeF& size)
{
    // setSize should not be used to reset the size
    Q_ASSERT(size.isValid());

    m_Size = size;
    performAction(IndexMetadata::GeometryAction::RESIZE);
}

StateTracker::Geometry::State StateTracker::Geometry::state() const
{
    return m_State;
}

enum Pos {Top, Left, Right, Bottom};
static Pos edgeToPos(Qt::Edge e)
{
    switch(e) {
        case Qt::TopEdge:
            return Pos::Top;
        case Qt::LeftEdge:
            return Pos::Left;
        case Qt::RightEdge:
            return Pos::Right;
        case Qt::BottomEdge:
            return Pos::Bottom;
    }

    Q_ASSERT(false);

    return {};
}

qreal StateTracker::Geometry::borderDecoration(Qt::Edge e) const
{
    return m_lBorderDecoration[edgeToPos(e)];
}

void StateTracker::Geometry::setBorderDecoration(Qt::Edge e, qreal r)
{
    const auto pos = edgeToPos(e);

    if (m_lBorderDecoration[pos] == r)
        return;

    m_lBorderDecoration[pos] = r;

    performAction(IndexMetadata::GeometryAction::DECORATE);
}
