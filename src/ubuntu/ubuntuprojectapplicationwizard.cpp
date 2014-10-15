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

#include "ubuntushared.h"
#include "ubuntuprojectapplicationwizard.h"
#include "ubuntuconstants.h"
#include "ubuntuproject.h"
#include "ubuntubzr.h"
#include "ubuntufirstrunwizard.h"

#include <coreplugin/mimedatabase.h>

#include <utils/qtcassert.h>
#include <utils/pathchooser.h>
#include <qtsupport/qtkitinformation.h>
#include <qtsupport/qtsupportconstants.h>
#include <projectexplorer/kitmanager.h>
#include <extensionsystem/pluginmanager.h>
#include <cmakeprojectmanager/cmakekitinformation.h>

#include <projectexplorer/kitinformation.h>
#include <projectexplorer/toolchain.h>

#include <QDir>


using namespace Ubuntu::Internal;

enum {
    debug = 0
};


UbuntuProjectApplicationWizard::UbuntuProjectApplicationWizard(ProjectType type)
    : m_type(type)
{
    setRequiredFeatures(requiredFeatures());
}

QWizard *UbuntuProjectApplicationWizard::createWizardDialog (QWidget *parent, const Core::WizardDialogParameters &wizardDialogParameters) const
{
    QTC_ASSERT(!parameters().isNull(), return 0);
    UbuntuProjectApplicationWizardDialog *projectDialog = new UbuntuProjectApplicationWizardDialog(parent,
                                                                                                   m_type ,
                                                                                                   wizardDialogParameters);
    projectDialog->addChrootSetupPage(12);
    projectDialog->addTargetSetupPage(13);

    initProjectWizardDialog(projectDialog,
                            wizardDialogParameters.defaultPath(),
                            wizardDialogParameters.extensionPages());

    QString maintainer = QStringLiteral("username");
    QString whoami     = QStringLiteral("maintainerName");
    UbuntuBzr *bzr = UbuntuBzr::instance();

    if(!bzr->isInitialized()) {
        bzr->initialize();
        bzr->waitForFinished();
    }

    if(bzr->isInitialized()) {
        maintainer = bzr->launchpadId();
        whoami     = bzr->whoami();
    }

    projectDialog->setField(QStringLiteral("ClickMaintainer"),whoami);
    projectDialog->setField(QStringLiteral("ClickDomain"),QString(QStringLiteral("com.ubuntu.developer.")+maintainer));
    return projectDialog;
}

bool UbuntuProjectApplicationWizard::postGenerateFiles(const QWizard *w, const Core::GeneratedFiles &l, QString *errorMessage)
{
    const UbuntuProjectApplicationWizardDialog *dialog = qobject_cast<const UbuntuProjectApplicationWizardDialog *>(w);

    // Generate user settings
    foreach (const Core::GeneratedFile &file, l)
        if (file.attributes() & Core::GeneratedFile::OpenProjectAttribute) {
            dialog->writeUserFile(file.path());
            break;
        }

    // Post-Generate: Open the projects/editors
    return ProjectExplorer::CustomProjectWizard::postGenerateOpen(l ,errorMessage);
}


Core::FeatureSet UbuntuProjectApplicationWizard::requiredFeatures() const
{
#ifdef Q_PROCESSOR_ARM
    return CustomProjectWizard::requiredFeatures();
#else
    return CustomProjectWizard::requiredFeatures()
            | Core::Feature(QtSupport::Constants::FEATURE_QMLPROJECT)
            | Core::Feature(QtSupport::Constants::FEATURE_QT_QUICK_2);
#endif
}

UbuntuProjectApplicationWizardDialog::UbuntuProjectApplicationWizardDialog(QWidget *parent,
                                                                           UbuntuProjectApplicationWizard::ProjectType type,
                                                                           const Core::WizardDialogParameters &parameters)
    : ProjectExplorer::BaseProjectWizardDialog(parent,parameters)
    , m_targetSetupPage(0)
    , m_type(type)
{
    init();
}

UbuntuProjectApplicationWizardDialog::~UbuntuProjectApplicationWizardDialog()
{
    if (m_targetSetupPage && !m_targetSetupPage->parent())
        delete m_targetSetupPage;
}

bool UbuntuProjectApplicationWizardDialog::writeUserFile(const QString &projectFileName) const
{
    if (!m_targetSetupPage)
        return false;

    QFileInfo fi = QFileInfo(projectFileName);
    if (!fi.exists())
        return false;

    QString filePath = fi.canonicalFilePath();

    if (const Core::MimeType mt = Core::MimeDatabase::findByFile(fi)) {
        QList<ProjectExplorer::IProjectManager*> allProjectManagers = ExtensionSystem::PluginManager::getObjects<ProjectExplorer::IProjectManager>();
        foreach (ProjectExplorer::IProjectManager *manager, allProjectManagers) {
            if (manager->mimeType() == mt.type()) {
                QString tmp;
                if (ProjectExplorer::Project *pro = manager->openProject(filePath, &tmp)) {
                    if(debug) qDebug()<<"Storing project type settings: "<<pro->id().toSetting();

                    bool success = m_targetSetupPage->setupProject(pro);
                    if(success) {
                        pro->saveSettings();
                    }
                    delete pro;
                    return success;
                }
                break;
            }
        }
    }
    return false;
}


void UbuntuProjectApplicationWizardDialog::init()
{
    setWindowTitle(tr("New Ubuntu Project"));
    setIntroDescription(tr("This wizard generates a Ubuntu project based on Ubuntu Components."));

    connect(this, SIGNAL(projectParametersChanged(QString,QString)),
            this, SLOT(generateProfileName(QString,QString)));
}

void UbuntuProjectApplicationWizardDialog::addChrootSetupPage(int id)
{
    QList<ProjectExplorer::Kit *> allKits = ProjectExplorer::KitManager::kits();

    bool found = false;
    foreach(ProjectExplorer::Kit *curr, allKits) {
        ProjectExplorer::ToolChain *tc = ProjectExplorer::ToolChainKitInformation::toolChain(curr);
        if (tc->type() == QLatin1String(Constants::UBUNTU_CLICK_TOOLCHAIN_ID)) {
            found = true;
            break;
        }
    }

    if(found)
        return;

    if (id >= 0)
        setPage(id, new UbuntuSetupChrootWizardPage);
    else
        id = addPage(new UbuntuSetupChrootWizardPage);
}

void UbuntuProjectApplicationWizardDialog::addTargetSetupPage(int id)
{
    m_targetSetupPage = new ProjectExplorer::TargetSetupPage;
    const QString platform = selectedPlatform();

    //prefer Qt Desktop or Platform Kit
    Core::FeatureSet features = Core::FeatureSet(QtSupport::Constants::FEATURE_DESKTOP);
    if (platform.isEmpty())
        m_targetSetupPage->setPreferredKitMatcher(new QtSupport::QtVersionKitMatcher(features));
    else
        m_targetSetupPage->setPreferredKitMatcher(new QtSupport::QtPlatformKitMatcher(platform));


    switch (m_type) {
        case UbuntuProjectApplicationWizard::CMakeProject:{
            m_targetSetupPage->setRequiredKitMatcher(new CMakeProjectManager::CMakeKitMatcher());
            break;
        }
        case UbuntuProjectApplicationWizard::GoProject: {
            ProjectExplorer::KitMatcher *matcher = 0;
            const char* retTypeName = QMetaType::typeName(qMetaTypeId<void*>());
            void **arg = reinterpret_cast<void**>(&matcher);
            ProjectExplorer::IProjectManager *manager = qobject_cast<ProjectExplorer::IProjectManager *>(ExtensionSystem::PluginManager::getObjectByClassName(QStringLiteral("GoLang::Internal::Manager")));
            if (manager) {
                bool success = QMetaObject::invokeMethod(manager,
                                                         "createKitMatcher",
                                                         QGenericReturnArgument(retTypeName,arg));
                if(!success && debug)
                     qDebug()<<"Invoke failed";
            }

            if (matcher)
                m_targetSetupPage->setRequiredKitMatcher(matcher);
            else
                //this is just a fallback for now to remove all ubuntu kits until cross compiling is sorted out
                //it should not be hit at all but i keep it there just to be safe
                m_targetSetupPage->setRequiredKitMatcher(new QtSupport::QtVersionKitMatcher(features));
            break;
        }
        case UbuntuProjectApplicationWizard::UbuntuQMLProject:
        case UbuntuProjectApplicationWizard::UbuntuHTMLProject:
        case UbuntuProjectApplicationWizard::UbuntuWebappProject: {
            m_targetSetupPage->setRequiredKitMatcher(new UbuntuKitMatcher());
            break;
        }
        default:
            break;
    }
    resize(900, 450);
    if (id >= 0)
        setPage(id, m_targetSetupPage);
    else
        id = addPage(m_targetSetupPage);

    wizardProgress()->item(id)->setTitle(tr("Kits"));
}

QList<Core::Id> UbuntuProjectApplicationWizardDialog::selectedKits() const
{
    if(m_targetSetupPage)
        return m_targetSetupPage->selectedKits();

    return QList<Core::Id>();
}

void UbuntuProjectApplicationWizardDialog::generateProfileName(const QString &projectName, const QString &path)
{
    if(!m_targetSetupPage)
        return;

    if(m_type == UbuntuProjectApplicationWizard::GoProject) {
        m_targetSetupPage->setProjectPath(path+QDir::separator()
                                          +projectName
                                          +QDir::separator()
                                          +QString::fromLatin1("%1.goproject").arg(projectName));
    } else if (m_type == UbuntuProjectApplicationWizard::UbuntuHTMLProject) {
        m_targetSetupPage->setProjectPath(path+QDir::separator()
                                          +projectName
                                          +QDir::separator()
                                          +QString::fromLatin1("%1.ubuntuhtmlproject").arg(projectName));
    } else if (m_type == UbuntuProjectApplicationWizard::UbuntuQMLProject) {
        m_targetSetupPage->setProjectPath(path+QDir::separator()
                                          +projectName
                                          +QDir::separator()
                                          +QString::fromLatin1("%1.qmlproject").arg(projectName));
    } else if (m_type == UbuntuProjectApplicationWizard::UbuntuWebappProject) {
        m_targetSetupPage->setProjectPath(path+QDir::separator()
                                          +projectName
                                          +QDir::separator()
                                          +QString::fromLatin1("%1.ubuntuwebappproject").arg(projectName));
    } else {
        m_targetSetupPage->setProjectPath(path+QDir::separator()+projectName+QDir::separator()+QLatin1String("CMakeLists.txt"));
    }
}
