/*
 * Copyright 2014 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Florian Boucault <florian.boucault@canonical.com>
 */

#ifndef UPMGRAPHMODEL_H
#define UPMGRAPHMODEL_H

#include <QtCore/QObject>
#include <QtGui/QImage>

class UPMGraphModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QImage image READ image NOTIFY imageChanged)
    Q_PROPERTY(int shift READ shift NOTIFY shiftChanged)
    Q_PROPERTY(int samples READ samples WRITE setSamples NOTIFY samplesChanged)

public:
    explicit UPMGraphModel(QObject *parent = 0);

    void appendValue(int width, int value);

    // getters
    QImage image() const;
    int shift() const;
    int samples() const;

    // setters
    void setSamples(int samples);

Q_SIGNALS:
    void imageChanged();
    void shiftChanged();
    void samplesChanged();

private:
    QImage m_image;
    int m_shift;
    int m_samples;
};

#endif // UPMGRAPHMODEL_H
