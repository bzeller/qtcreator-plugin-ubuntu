#include "ubuntudirectuploadstep.h"
#include "ubuntupackagestep.h"
#include "ubuntuconstants.h"
#include "ubuntudevice.h"
#include "ubunturemotedeployconfiguration.h"
#include "ubuntuwaitfordevicedialog.h"

#include <utils/qtcassert.h>
#include <projectexplorer/deployablefile.h>
#include <projectexplorer/target.h>
#include <projectexplorer/project.h>
#include <projectexplorer/devicesupport/devicemanager.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/buildsteplist.h>

#include <remotelinux/genericdirectuploadservice.h>

#include <QFileInfo>

namespace Ubuntu {
namespace Internal {

enum {
    debug = 0
};

static UbuntuDevice::ConstPtr deviceFromTarget (ProjectExplorer::Target *target)
{
    if(!target || !target->kit())
        return UbuntuDevice::ConstPtr();

    ProjectExplorer::IDevice::ConstPtr dev = ProjectExplorer::DeviceKitInformation::device(target->kit());
    if(!dev)
        return UbuntuDevice::ConstPtr();

    if(!dev->type().toString().startsWith(QLatin1String(Constants::UBUNTU_DEVICE_TYPE_ID)))
        return UbuntuDevice::ConstPtr();

    return qSharedPointerCast<const UbuntuDevice>(dev);
}

UbuntuDirectUploadStep::UbuntuDirectUploadStep(ProjectExplorer::BuildStepList *bsl)
    : AbstractRemoteLinuxDeployStep(bsl, UbuntuDirectUploadStep::stepId())
    , m_deployService(new RemoteLinux::GenericDirectUploadService(this))
    , m_future(0)
{
    setDefaultDisplayName(displayName());
}

UbuntuDirectUploadStep::UbuntuDirectUploadStep(ProjectExplorer::BuildStepList *bsl, UbuntuDirectUploadStep *other)
    : AbstractRemoteLinuxDeployStep(bsl, other)
    , m_deployService(new RemoteLinux::GenericDirectUploadService(this))
    , m_future(0)
{
    setDefaultDisplayName(displayName());

    connect(target()->project(),SIGNAL(displayNameChanged()),this,SLOT(projectNameChanged()));
    connect(target(),SIGNAL(applicationTargetsChanged()),this,SLOT(projectNameChanged()));
}

UbuntuDirectUploadStep::~UbuntuDirectUploadStep()
{
}

void UbuntuDirectUploadStep::run(QFutureInterface<bool> &fi)
{
    m_foundClickPackage = false;
    projectNameChanged();
    if(!m_foundClickPackage) {
        emit addOutput(tr("Deploy step failed. No click package was created"), ErrorMessageOutput);
        fi.reportResult(false);
        emit finished();
        return;
    }


    m_deployService->setIncrementalDeployment(false);
    m_deployService->setIgnoreMissingFiles(false);

    UbuntuDevice::ConstPtr dev = deviceFromTarget(target());
    if(!dev) {
        emit addOutput(tr("Deploy step failed. No valid device configured"), ErrorMessageOutput);
        fi.reportResult(false);
        emit finished();
        return;
    }

    m_future = &fi;
    if(dev->deviceState() != ProjectExplorer::IDevice::DeviceReadyToUse) {
        //we are already waiting
        if (m_waitDialog)
            return;

        m_waitDialog = new UbuntuWaitForDeviceDialog(Core::ICore::mainWindow());
        connect(m_waitDialog.data(),SIGNAL(canceled()),this,SLOT(handleWaitDialogCanceled()));
        connect(m_waitDialog.data(),SIGNAL(deviceReady()),this,SLOT(handleDeviceReady()));
        m_waitDialog->show(dev);

        if(dev->machineType() == ProjectExplorer::IDevice::Emulator && dev->deviceState() == ProjectExplorer::IDevice::DeviceDisconnected)
            dev->helper()->device()->startEmulator();

    } else {
        handleDeviceReady();
    }
}

void UbuntuDirectUploadStep::handleWaitDialogCanceled( )
{
    m_waitDialog->deleteLater();

    emit addOutput(tr("Deploy step failed"), ErrorMessageOutput);
    m_future->reportResult(false);
    m_future = 0;
    emit finished();
}

void UbuntuDirectUploadStep::handleDeviceReady()
{
    m_waitDialog->deleteLater();

    QString whyNot;
    if(!deployService()->isDeploymentPossible(&whyNot)) {
        emit addOutput(tr("Deploy step failed. %1").arg(whyNot), ErrorMessageOutput);
        m_future->reportResult(false);
        m_future = 0;
        emit finished();
        return;
    }

    AbstractRemoteLinuxDeployStep::run(*m_future);
    m_future = 0;
}

ProjectExplorer::BuildStepConfigWidget *UbuntuDirectUploadStep::createConfigWidget()
{
    return new ProjectExplorer::SimpleBuildStepConfigWidget(this);
}

bool UbuntuDirectUploadStep::initInternal(QString *error)
{
    Q_UNUSED(error)
    return true;
}

RemoteLinux::AbstractRemoteLinuxDeployService *UbuntuDirectUploadStep::deployService() const
{
    return m_deployService;
}

bool UbuntuDirectUploadStep::fromMap(const QVariantMap &map)
{
    if (!AbstractRemoteLinuxDeployStep::fromMap(map))
        return false;
    return true;
}

QVariantMap UbuntuDirectUploadStep::toMap() const
{
    return AbstractRemoteLinuxDeployStep::toMap();
}


Core::Id UbuntuDirectUploadStep::stepId()
{
    return Constants::UBUNTU_DEPLOY_UPLOADSTEP_ID;
}

QString UbuntuDirectUploadStep::displayName()
{
    return tr("Upload files to Ubuntu Device");
}

void UbuntuDirectUploadStep::projectNameChanged()
{
    if(debug) qDebug()<<"------------------------ Updating DEPLOYLIST ---------------------------";

    ProjectExplorer::BuildStepList *bsList = qobject_cast<UbuntuRemoteDeployConfiguration *>(BuildStep::deployConfiguration())->stepList();
    QList<ProjectExplorer::DeployableFile> list;
    foreach(ProjectExplorer::BuildStep *currStep ,bsList->steps()) {
        UbuntuPackageStep *pckStep = qobject_cast<UbuntuPackageStep*>(currStep);
        if(!pckStep)
            continue;

        QFileInfo info(pckStep->packagePath());
        if(info.exists()) {
            list.append(ProjectExplorer::DeployableFile(info.filePath(),
                                                        QStringLiteral("/tmp")));

            list.append(ProjectExplorer::DeployableFile(QStringLiteral("%1/qtc_device_applaunch.py").arg(Constants::UBUNTU_SCRIPTPATH),
                                                        QStringLiteral("/tmp")));
            m_deployService->setDeployableFiles(list);
            m_foundClickPackage = true;
            break;
        }
    }
}
} // namespace Internal
} // namespace Ubuntu
