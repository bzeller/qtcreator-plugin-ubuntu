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

#ifndef UBUNTUPROJECTAPPLICATIONWIZARD_H
#define UBUNTUPROJECTAPPLICATIONWIZARD_H

#include <coreplugin/basefilewizard.h>
#include <projectexplorer/baseprojectwizarddialog.h>
#include <qt4projectmanager/qt4project.h>
#include <qmlprojectmanager/qmlproject.h>
#include <extensionsystem/pluginmanager.h>

#include <QJsonObject>
#include "ubuntuprojectapp.h"

namespace Ubuntu {
namespace Internal {

class UbuntuProjectApplicationWizardDialog : public ProjectExplorer::BaseProjectWizardDialog
{
    Q_OBJECT
public:
    explicit UbuntuProjectApplicationWizardDialog(QWidget *parent,
                                               const Core::WizardDialogParameters &parameters);
};

class UbuntuProjectApplicationWizard : public Core::BaseFileWizard
{
    Q_OBJECT

public:
    UbuntuProjectApplicationWizard(QJsonObject);
    virtual ~UbuntuProjectApplicationWizard();
    virtual Core::FeatureSet requiredFeatures() const;

    Core::BaseFileWizardParameters parameters(QJsonObject);

    static QByteArray getProjectTypesJSON();

    static QString templatesPath(QString fileName);

protected:
    virtual QWizard *createWizardDialog(QWidget *parent,
                                        const Core::WizardDialogParameters &wizardDialogParameters) const;

    virtual Core::GeneratedFiles generateFiles(const QWizard *w,
                                               QString *errorMessage) const;

    virtual bool postGenerateFiles(const QWizard *w, const Core::GeneratedFiles &l, QString *errorMessage);

private:
    UbuntuProjectApp* m_app;
};

} // Internal
} // Ubuntu


#endif // UBUNTUPROJECTAPPLICATIONWIZARD_H