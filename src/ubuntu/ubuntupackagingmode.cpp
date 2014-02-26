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

#include "ubuntupackagingmode.h"
#include "ubuntuconstants.h"

#include <coreplugin/modemanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/dialogs/iwizard.h>
#include <coreplugin/coreconstants.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/iprojectmanager.h>
#include <projectexplorer/session.h>
#include <projectexplorer/project.h>
#include <coreplugin/icore.h>
#include <coreplugin/imode.h>
#include <utils/styledbar.h>
#include <QVBoxLayout>
#include <QScrollArea>

#include <cmakeprojectmanager/cmakeprojectconstants.h>

using namespace Ubuntu::Internal;

UbuntuPackagingMode::UbuntuPackagingMode(QObject *parent) :
    Core::IMode(parent)
{
    setDisplayName(tr(Ubuntu::Constants::UBUNTU_MODE_PACKAGING_DISPLAYNAME));
    setIcon(QIcon(QLatin1String(Ubuntu::Constants::UBUNTU_MODE_PACKAGING_ICON)));
    setPriority(Ubuntu::Constants::UBUNTU_MODE_PACKAGING_PRIORITY);
    setId(Ubuntu::Constants::UBUNTU_MODE_PACKAGING);
    setObjectName(QLatin1String(Ubuntu::Constants::UBUNTU_MODE_PACKAGING));

    m_modeWidget = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout;
    layout->setMargin(0);
    layout->setSpacing(0);
    m_modeWidget->setLayout(layout);


    Utils::StyledBar* styledBar = new Utils::StyledBar(m_modeWidget);
    layout->addWidget(styledBar);
    QScrollArea *scrollArea = new QScrollArea(m_modeWidget);
    scrollArea->setFrameShape(QFrame::NoFrame);
    layout->addWidget(scrollArea);
    scrollArea->setWidget(&m_ubuntuPackagingWidget);
    scrollArea->setWidgetResizable(true);

    connect(Core::ModeManager::instance(), SIGNAL(currentModeChanged(Core::IMode*)), SLOT(modeChanged(Core::IMode*)));

    ProjectExplorer::SessionManager* sessionManager = ProjectExplorer::ProjectExplorerPlugin::instance()->session();
    connect(sessionManager,SIGNAL(projectAdded(ProjectExplorer::Project*)),SLOT(on_projectAdded(ProjectExplorer::Project*)));
    connect(sessionManager,SIGNAL(projectRemoved(ProjectExplorer::Project*)),SLOT(on_projectRemoved(ProjectExplorer::Project*)));
    connect(sessionManager,SIGNAL(startupProjectChanged(ProjectExplorer::Project*)),SLOT(on_projectAdded(ProjectExplorer::Project*)));
    connect(&m_ubuntuPackagingWidget,SIGNAL(reviewToolsInstalledChanged(bool)),this,SLOT(updateModeState()));

    setWidget(m_modeWidget);
    setEnabled(false);
}

void UbuntuPackagingMode::initialize() {

}

void UbuntuPackagingMode::modeChanged(Core::IMode* currentMode) {
    if (currentMode->id() == id()) {
        ProjectExplorer::ProjectExplorerPlugin* projectExplorerInstance = ProjectExplorer::ProjectExplorerPlugin::instance();
        ProjectExplorer::Project* startupProject = projectExplorerInstance->startupProject();

        bool isQmlProject = false;
        bool isQmakeProject = false;
        bool isUbuntuProject = false;
        bool isCMakeProject = false;

        if (startupProject) {
            isQmlProject = (startupProject->projectManager()->mimeType() == QLatin1String(Constants::QMLPROJECT_MIMETYPE));
            isQmakeProject = (startupProject->projectManager()->mimeType() == QLatin1String(Constants::QMAKE_MIMETYPE));
            isUbuntuProject = (startupProject->projectManager()->mimeType() == QLatin1String(Constants::UBUNTUPROJECT_MIMETYPE));
            isCMakeProject  = (startupProject->projectManager()->mimeType() == QLatin1String(CMakeProjectManager::Constants::CMAKEMIMETYPE));
        }

        if (isQmlProject || isUbuntuProject || isCMakeProject) {
            m_ubuntuPackagingWidget.openManifestForProject();
            m_ubuntuPackagingWidget.setAvailable(true);
        } else {
            m_ubuntuPackagingWidget.setAvailable(false);
        }

    } else if (previousMode == id()) {
        m_ubuntuPackagingWidget.save();
    }

    previousMode = currentMode->id();
}

void UbuntuPackagingMode::updateModeState() {
    ProjectExplorer::SessionManager* sessionManager = ProjectExplorer::ProjectExplorerPlugin::instance()->session();
    ProjectExplorer::ProjectExplorerPlugin* projectExplorerInstance = ProjectExplorer::ProjectExplorerPlugin::instance();
    ProjectExplorer::Project* startupProject = projectExplorerInstance->startupProject();

    bool isQmlProject = false;
    bool isQmakeProject = false;
    bool isUbuntuProject = false;
    bool isCMakeProject = false;
    bool reviewToolsInstalled = m_ubuntuPackagingWidget.reviewToolsInstalled();

    if (startupProject) {
        isQmlProject = (startupProject->projectManager()->mimeType() == QLatin1String(Constants::QMLPROJECT_MIMETYPE));
        isQmakeProject = (startupProject->projectManager()->mimeType() == QLatin1String(Constants::QMAKE_MIMETYPE));
        isUbuntuProject = (startupProject->projectManager()->mimeType() == QLatin1String(Constants::UBUNTUPROJECT_MIMETYPE));
        isCMakeProject  = (startupProject->projectManager()->mimeType() == QLatin1String(CMakeProjectManager::Constants::CMAKEMIMETYPE));
    }

    this->setEnabled((isQmlProject || isUbuntuProject || isCMakeProject || reviewToolsInstalled));
    if (this->isEnabled()) {
        m_ubuntuPackagingWidget.openManifestForProject();
        m_ubuntuPackagingWidget.save();
    }
}

void UbuntuPackagingMode::on_projectAdded(ProjectExplorer::Project *project) {
    Q_UNUSED(project);
    updateModeState();
}

void UbuntuPackagingMode::on_projectRemoved(ProjectExplorer::Project *project) {
    Q_UNUSED(project);
    updateModeState();
}