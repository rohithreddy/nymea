/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *                                                                         *
 *  Copyright (C) 2015 Simon Stürz <simon.stuerz@guh.io>                   *
 *  Copyright (C) 2014 Michael Zanetti <michael_zanetti@gmx.net>           *
 *                                                                         *
 *  This file is part of nymea.                                            *
 *                                                                         *
 *  This library is free software; you can redistribute it and/or          *
 *  modify it under the terms of the GNU Lesser General Public             *
 *  License as published by the Free Software Foundation; either           *
 *  version 2.1 of the License, or (at your option) any later version.     *
 *                                                                         *
 *  This library is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU      *
 *  Lesser General Public License for more details.                        *
 *                                                                         *
 *  You should have received a copy of the GNU Lesser General Public       *
 *  License along with this library; If not, see                           *
 *  <http://www.gnu.org/licenses/>.                                        *
 *                                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef TRIGGERTYPE_H
#define TRIGGERTYPE_H

#include "libnymea.h"
#include "typeutils.h"
#include "paramtype.h"

#include <QVariantMap>

class LIBNYMEA_EXPORT EventType
{
public:
    EventType(const EventTypeId &id);

    EventTypeId id() const;

    QString name() const;
    void setName(const QString &name);

    QString displayName() const;
    void setDisplayName(const QString &displayName);

    int index() const;
    void setIndex(const int &index);

    ParamTypes paramTypes() const;
    void setParamTypes(const ParamTypes &paramTypes);

    bool ruleRelevant() const;
    void setRuleRelevant(const bool &ruleRelevant);

    bool graphRelevant() const;
    void setGraphRelevant(const bool &graphRelevant);

    bool isValid() const;

    static QStringList typeProperties();
    static QStringList mandatoryTypeProperties();

private:
    EventTypeId m_id;
    QString m_name;
    QString m_displayName;
    int m_index;
    QList<ParamType> m_paramTypes;
    bool m_ruleRelevant;
    bool m_graphRelevant;
};

class EventTypes: public QList<EventType>
{
public:
    EventTypes() = default;
    EventTypes(const QList<EventType> &other);
    EventType findByName(const QString &name);
    EventType findById(const EventTypeId &id);
};

#endif // TRIGGERTYPE_H
