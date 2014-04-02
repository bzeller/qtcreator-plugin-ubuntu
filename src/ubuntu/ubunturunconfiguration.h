/*
 * Copyright 2013 Canonical Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 2.1.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Juhapekka Piiroinen <juhapekka.piiroinen@canonical.com>
 */

#ifndef UBUNTURUNCONFIGURATION_H
#define UBUNTURUNCONFIGURATION_H

#include <QObject>
#include <projectexplorer/localapplicationrunconfiguration.h>


namespace ProjectExplorer {
class Target;
}


namespace Ubuntu {
namespace Internal {

class UbuntuRunConfiguration : public ProjectExplorer::LocalApplicationRunConfiguration
{
    Q_OBJECT
public:
    UbuntuRunConfiguration(ProjectExplorer::Target *parent, Core::Id id);
    UbuntuRunConfiguration(ProjectExplorer::Target *parent, UbuntuRunConfiguration* source);

    QWidget *createConfigurationWidget();
    bool isEnabled() const;

    // LocalApplicationRunConfiguration interface
    virtual QString executable() const;
    virtual QString workingDirectory() const;
    virtual QString commandLineArguments() const;
    virtual QString dumperLibrary() const;
    virtual QStringList dumperLibraryLocations() const;
    virtual RunMode runMode() const;
};

}
}

#endif // UBUNTURUNCONFIGURATION_H
