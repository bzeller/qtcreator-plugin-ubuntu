/*
 * Copyright 2014 Canonical Ltd.
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
 * Author: Benjamin Zeller <benjamin.zeller@canonical.com>
 */

#include "ubuntuclicktool.h"
#include "ubuntuclickmanifest.h"
#include "ubuntuconstants.h"
#include "ubuntushared.h"
#include "clicktoolchain.h"

#include <QRegularExpression>
#include <QDir>
#include <QMessageBox>
#include <QInputDialog>
#include <QProcess>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <coreplugin/icore.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/target.h>
#include <projectexplorer/project.h>
#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/kitinformation.h>
#include <utils/qtcprocess.h>
#include <utils/environment.h>
#include <utils/consoleprocess.h>

#include <cmakeprojectmanager/cmakeprojectconstants.h>
#include <cmakeprojectmanager/cmakebuildconfiguration.h>

#include <QDebug>

namespace Ubuntu {
namespace Internal {

enum {
    debug = 0
};

static QString getChrootSuffixFromEnv ()
{
    QByteArray value = qgetenv(Constants::UBUNTU_CLICK_CHROOT_SUFFIX_ENV_VAR);
    if(value.isNull())
        return QLatin1String(Constants::UBUNTU_CLICK_CHROOT_DEFAULT_NAME);

    return QString::fromLatin1(value);
}

/**
* Initialize the m_strClickChrootSuffix from the environment variable
*/
QString UbuntuClickTool::m_strClickChrootSuffix = getChrootSuffixFromEnv();

/**
 * @brief UbuntuClickTool::UbuntuClickTool
 * Implements functionality needed for executing the click
 * tool
 */
UbuntuClickTool::UbuntuClickTool()
{
}

/**
 * @brief UbuntuClickTool::clickChrootSuffix
 * Returns the click chroot suffix to be used with click operations
 */
QString UbuntuClickTool::clickChrootSuffix()
{
    return m_strClickChrootSuffix;
}

/**
 * @brief UbuntuClickTool::parametersForCreateChroot
 * Initializes a ProjectExplorer::ProcessParameters object with command and arguments
 * to create a new chroot
 */
void UbuntuClickTool::parametersForCreateChroot(const Target &target, ProjectExplorer::ProcessParameters *params)
{
    QString command = QString::fromLatin1(Constants::UBUNTU_CLICK_CHROOT_CREATE_ARGS)
            .arg(Constants::UBUNTU_SCRIPTPATH)
            .arg(target.architecture)
            .arg(target.framework)
            .arg(target.series)
            .arg(clickChrootSuffix());


    QSettings settings(QLatin1String(Constants::SETTINGS_COMPANY),QLatin1String(Constants::SETTINGS_PRODUCT));
    settings.beginGroup(QLatin1String(Constants::SETTINGS_GROUP_CLICK));
    bool useLocalMirror = settings.value(QLatin1String(Constants::SETTINGS_KEY_CHROOT_USE_LOCAL_MIRROR),false).toBool();
    settings.endGroup();

    if(!useLocalMirror)
        command.prepend(QStringLiteral("env CLICK_NO_LOCAL_MIRROR=1 "));

    params->setCommand(QLatin1String(Constants::UBUNTU_SUDO_BINARY));
    params->setEnvironment(Utils::Environment::systemEnvironment());
    params->setArguments(command);
}

/**
 * @brief UbuntuClickTool::parametersForMaintainChroot
 * Initializes params with the arguments for maintaining the chroot
 * @note does not call ProjectExplorer::ProcessParameters::resolveAll()
 */
void UbuntuClickTool::parametersForMaintainChroot(const UbuntuClickTool::MaintainMode &mode, const Target &target, ProjectExplorer::ProcessParameters *params)
{
    QString arguments;
    switch (mode) {
        case Upgrade:
            params->setCommand(QLatin1String(Constants::UBUNTU_CLICK_BINARY));
            arguments = QString::fromLatin1(Constants::UBUNTU_CLICK_CHROOT_UPGRADE_ARGS)
                    .arg(target.architecture)
                    .arg(target.framework)
                    .arg(target.series)
                    .arg(clickChrootSuffix());
            break;
        case Delete:
            params->setCommand(QLatin1String(Constants::UBUNTU_SUDO_BINARY));
            arguments = QString::fromLatin1(Constants::UBUNTU_CLICK_CHROOT_DESTROY_ARGS)
                    .arg(Constants::UBUNTU_SCRIPTPATH)
                    .arg(target.architecture)
                    .arg(target.framework)
                    .arg(target.series)
                    .arg(clickChrootSuffix());
            break;
    }


    params->setEnvironment(Utils::Environment::systemEnvironment());
    params->setArguments(arguments);
}

/**
 * @brief UbuntuClickTool::openChrootTerminal
 * Opens a new terminal logged into the chroot specified by \a target
 * The terminal emulator used is specified in the Creator environment option page
 */
void UbuntuClickTool::openChrootTerminal(const UbuntuClickTool::Target &target)
{
    QStringList args = Utils::QtcProcess::splitArgs(Utils::ConsoleProcess::terminalEmulator(Core::ICore::settings()));
    QString     term = args.takeFirst();

    args << QString(QLatin1String(Constants::UBUNTU_CLICK_OPEN_TERMINAL))
            .arg(target.architecture)
            .arg(target.framework)
            .arg(target.series)
            .arg(clickChrootSuffix());

    if(!QProcess::startDetached(term,args,QDir::homePath())) {
        printToOutputPane(QLatin1String(Constants::UBUNTU_CLICK_OPEN_TERMINAL_ERROR));
    }
}

bool UbuntuClickTool::getTargetFromUser(Target *target, const QString &framework)
{
    QList<UbuntuClickTool::Target> targets = UbuntuClickTool::listAvailableTargets(framework);
    if (!targets.size()) {
        QString message = QCoreApplication::translate("UbuntuClickTool",Constants::UBUNTU_CLICK_NOTARGETS_MESSAGE);
        if(!framework.isEmpty()) {
            message = QCoreApplication::translate("UbuntuClickTool",Constants::UBUNTU_CLICK_NOTARGETS_FRAMEWORK_MESSAGE)
                    .arg(framework);
        }

        QMessageBox::warning(Core::ICore::mainWindow(),
                             QCoreApplication::translate("UbuntuClickTool",Constants::UBUNTU_CLICK_NOTARGETS_TITLE),
                             message);
        return false;
    }

    //if we have only 1 target there is nothing to choose
    if(targets.size() == 1){
        *target = targets[0];
        return true;
    }

    QStringList items;
    foreach(const UbuntuClickTool::Target& t, targets)
        items << QString::fromLatin1("%0-%1").arg(t.framework).arg(t.architecture);

    bool ok = false;
    QString item = QInputDialog::getItem(Core::ICore::mainWindow()
                                         ,QCoreApplication::translate("UbuntuClickTool",Constants::UBUNTU_CLICK_SELECT_TARGET_TITLE)
                                         ,QCoreApplication::translate("UbuntuClickTool",Constants::UBUNTU_CLICK_SELECT_TARGET_LABEL)
                                         ,items,0,false,&ok);
    //get index of item in the targets list
    int idx = items.indexOf(item);
    if(!ok || idx < 0 || idx >= targets.size())
        return false;

    *target = targets[idx];
    return true;
}

QString UbuntuClickTool::targetBasePath(const UbuntuClickTool::Target &target)
{
    return QString::fromLatin1("%1/%2-%3-%4")
            .arg(QLatin1String(Constants::UBUNTU_CLICK_CHROOT_BASEPATH))
	    .arg(clickChrootSuffix())
            .arg(target.framework)
            .arg(target.architecture);
}

/*!
 * \brief UbuntuClickTool::targetExists
 * checks if the target is still available
 */
bool UbuntuClickTool::targetExists(const UbuntuClickTool::Target &target)
{
    QPair<int,int> targetVer = targetVersion(target);
    if(targetVer.first == -1)
        return false;

    return true;
}

/**
 * @brief UbuntuClickTool::listAvailableTargets
 * @return all currently existing chroot targets in the system
 */
QList<UbuntuClickTool::Target> UbuntuClickTool::listAvailableTargets(const QString &framework)
{
    QList<Target> items;
    QDir chrootDir(QLatin1String(Constants::UBUNTU_CLICK_CHROOT_BASEPATH));
    QString filterRegex;
    filterRegex = QString::fromLatin1(Constants::UBUNTU_CLICK_TARGETS_REGEX).arg(clickChrootSuffix());
    if(!framework.isEmpty()) {
        QRegularExpression expr(QLatin1String(Constants::UBUNTU_CLICK_BASE_FRAMEWORK_REGEX));
        QRegularExpressionMatch match = expr.match(framework);
        if(match.hasMatch()) {
            if(debug) qDebug()<<"Filtering for base framework: "<<match.captured(1);
            filterRegex = QString::fromLatin1(Constants::UBUNTU_CLICK_TARGETS_FRAMEWORK_REGEX)
                        		     .arg(clickChrootSuffix())
                                             .arg(match.captured(1));
        }
    }

    //if the dir does not exist there are no available chroots
    if(!chrootDir.exists())
        return items;

    QStringList availableChroots = chrootDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                       QDir::Name | QDir::Reversed);

    QRegularExpression clickFilter(filterRegex);
    //iterate over all chroots and check if they are click chroots
    foreach (const QString &chroot, availableChroots) {
        QRegularExpressionMatch match = clickFilter.match(chroot);
        if(!match.hasMatch()) {
            continue;
        }

        Target t;
        if(!targetFromPath(chroot,&t))
            continue;

        items.append(t);
    }
    return items;
}

/**
 * @brief UbuntuClickTool::targetVersion
 * Reads the ubuntu version from the lsb-release file
 * @returns a QPair containing the major and minor version information
 */
QPair<int, int> UbuntuClickTool::targetVersion(const UbuntuClickTool::Target &target)
{
    QFile f(QString::fromLatin1("%1/%2")
            .arg(targetBasePath(target))
            .arg(QLatin1String("etc/lsb-release")));

    if (!f.open(QIODevice::ReadOnly)) {
        //there is no lsb-release file... what now?
        return qMakePair(-1,-1);
    }

    QString info = QString::fromLatin1(f.readAll());

    QRegularExpression grep(QLatin1String(Constants::UBUNTU_CLICK_VERSION_REGEX),QRegularExpression::MultilineOption);
    QRegularExpressionMatch match = grep.match(info);

    if(!match.hasMatch()) {
        return qMakePair(-1,-1);
    }

    bool ok = false;
    int majorV = match.captured(1).toInt(&ok);
    if(!ok) {
        return qMakePair(-1,-1);
    }

    int minorV = match.captured(2).toInt();

    return qMakePair(majorV,minorV);
}

/*!
 * \brief UbuntuClickTool::targetFromPath
 * returns true if the given path is a click target
 * if it is, \a tg will be initialized with that targets values
 */
bool UbuntuClickTool::targetFromPath(const QString &targetPath, UbuntuClickTool::Target *tg)
{
    QRegularExpression clickFilter(QString::fromLatin1(Constants::UBUNTU_CLICK_TARGETS_REGEX).arg(clickChrootSuffix()));
    QRegularExpressionMatch match = clickFilter.match(targetPath);
    if(!match.hasMatch()) {
        return false;
    }

    Target t;
    t.maybeBroken  = false; //we are optimistic
    t.framework    = match.captured(1);
    t.architecture = match.captured(2);

    //now read informations about the target
    QFile f(QString::fromLatin1("%1/%2")
            .arg(targetBasePath(t))
            .arg(QLatin1String("/etc/lsb-release")));

    if (!f.open(QIODevice::ReadOnly)) {
        //there is no lsb-release file... what now?
        t.maybeBroken = true;

    } else {
        QString info = QString::fromLatin1(f.readAll());

        //read version
        QRegularExpression grep(QLatin1String(Constants::UBUNTU_CLICK_VERSION_REGEX),QRegularExpression::MultilineOption);
        QRegularExpressionMatch match = grep.match(info);

        if(!match.hasMatch()) {
            t.maybeBroken = true;
        } else {
            bool ok = false;

            t.majorVersion = match.captured(1).toInt(&ok);
            if(!ok) {
                t.maybeBroken = true;
                t.majorVersion = -1;
            }

            t.minorVersion = match.captured(2).toInt(&ok);
            if(!ok) {
                t.maybeBroken = true;
                t.minorVersion = -1;
            }
        }

        //read series
        grep.setPattern(QString::fromLatin1(Constants::UBUNTU_CLICK_SERIES_REGEX));
        grep.setPatternOptions(QRegularExpression::MultilineOption);
        match = grep.match(info);

        if(!match.hasMatch()) {
            t.maybeBroken = true;
        } else {
            t.series = match.captured(1);
        }
    }

    *tg = t;
    return true;
}

/*!
 * \brief UbuntuClickTool::clickTargetFromTarget
 * Tries to get the Click target from a projectconfiguration,
 * \returns 0 if nothing was found
 */
const UbuntuClickTool::Target *UbuntuClickTool::clickTargetFromTarget(ProjectExplorer::Target *t)
{
#ifndef IN_TEST_PROJECT
    if(!t)
        return 0;

    ProjectExplorer::ToolChain *tc = ProjectExplorer::ToolChainKitInformation::toolChain(t->kit());
    if(!tc || (tc->type() != QLatin1String(Constants::UBUNTU_CLICK_TOOLCHAIN_ID)))
        return 0;

    ClickToolChain *clickTc = static_cast<ClickToolChain*>(tc);
    if(!clickTc)
        return 0;

    return  &clickTc->clickTarget();
#else
    Q_UNUSED(t);
    return 0;
#endif
}

QString UbuntuClickTool::findOrCreateGccWrapper (const UbuntuClickTool::Target &target)
{
    QString compiler;

    if(target.architecture == QStringLiteral("armhf"))
        compiler = QStringLiteral("arm-linux-gnueabihf-gcc");
    else if(target.architecture == QStringLiteral("i386"))
        compiler = QStringLiteral("i686-linux-gnu-gcc");
    else if(target.architecture == QStringLiteral("amd64"))
        compiler = QStringLiteral("x86_64-linux-gnu-gcc");
    else {
        qWarning()<<"Invalid architecture, can not create gcc wrapper link";
        return QString();
    }

    return UbuntuClickTool::findOrCreateToolWrapper(compiler,target);
}

QString UbuntuClickTool::findOrCreateQMakeWrapper (const UbuntuClickTool::Target &target)
{
    QString qmake;

    if(target.architecture == QStringLiteral("armhf"))
        qmake = QStringLiteral("qt5-qmake-arm-linux-gnueabihf");
    else
        qmake = QStringLiteral("qmake");

    return UbuntuClickTool::findOrCreateToolWrapper(qmake,target);
}

QString UbuntuClickTool::findOrCreateMakeWrapper (const UbuntuClickTool::Target &target)
{
    return UbuntuClickTool::findOrCreateToolWrapper(QStringLiteral("make"),target);
}

QString UbuntuClickTool::findOrCreateToolWrapper (const QString &tool, const UbuntuClickTool::Target &target)
{
    QString baseDir = QStringLiteral("%1/ubuntu-sdk/%2-%3").arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation))
            .arg(target.framework)
            .arg(target.architecture);

    QDir d(baseDir);
    if(!d.exists()) {
        if(!d.mkpath(baseDir)){
            qWarning()<<"Could not create config directory.";
            return QString();
        }
    }

    QString toolWrapper = (Utils::FileName::fromString(baseDir).appendPath(tool).toString());
    QString toolTarget  = QString::fromLatin1(Constants::UBUNTU_CLICK_CHROOT_WRAPPER).arg(Constants::UBUNTU_SCRIPTPATH);

    QFileInfo symlinkInfo(toolWrapper);

    if(!symlinkInfo.exists() || toolTarget != symlinkInfo.symLinkTarget()) {
        //in case of a broken link QFile::exists also will return false
        //lets try to delete it and ignore the error in case the file
        //simply does not exist
        QFile::remove(toolWrapper);
        if(!QFile::link(toolTarget,toolWrapper)) {
            qWarning()<<"Unable to create link for the tool wrapper: "<<toolWrapper;
            return QString();
        }

    }
    return toolWrapper;
}

QDebug operator<<(QDebug dbg, const UbuntuClickTool::Target& t)
{
    dbg.nospace() << "("<<"series: "<<t.series<<" "
                        <<"arch: "<<t.architecture<<" "
                        <<"framework: "<<t.framework<<" "
                        <<"version: "<<t.majorVersion<<"."<<t.minorVersion<<" "
                        <<"broken "<<t.maybeBroken
                        <<")";

    return dbg.space();
}


/*!
 * \class UbuntuClickFrameworkProvider::UbuntuClickFrameworkProvider
 *
 * The UbuntuClickFrameworkProvider makes sure the IDE knows the most recent
 * framework list. It queries the Ubuntu servers for the list and fires frameworksUpdated()
 * if a new list is available. Widgets showing the frameworks should update accordingly
 *
 * The framework -> policy mapping is still hardcoded because there is no current API to query
 * it
 *
 * \note If no network connection is available the cache falls back to the last successful query or
 *       the shipped framework list in the resource file
 */


struct FrameworkDesc{
    FrameworkDesc() : develVersion(INT_MAX) {}
    QString base;
    QString baseVersion;
    QString sub;
    int develVersion;
};

//this is a horrible complicated sorting function, it should be replaced with something more trivial
static FrameworkDesc fwDescFromString (const QString &fw)
{
    FrameworkDesc fwDesc;

    QStringList ext;
    fwDesc.base = UbuntuClickFrameworkProvider::getBaseFramework(fw,&ext);

    int lastDash = fwDesc.base.lastIndexOf(QStringLiteral("-"));
    if(lastDash > 0) {
        fwDesc.baseVersion = fwDesc.base.mid(lastDash+1);
        fwDesc.base        = fwDesc.base.mid(0,lastDash);
    }

    QString sub;
    while(ext.size()) {
        sub = ext.takeFirst();
        if(sub.startsWith(QStringLiteral("dev"))) {
            fwDesc.develVersion = sub.remove(QStringLiteral("dev")).toInt();
        } else {
            fwDesc.sub = sub;
        }
    }
    return fwDesc;
}


static bool caseInsensitiveFWLessThan(const QString &s1, const QString &s2)
{

    FrameworkDesc fwDesc1 = fwDescFromString(s1);
    FrameworkDesc fwDesc2 = fwDescFromString(s2);

    int comp = QString::compare(fwDesc1.baseVersion,fwDesc2.baseVersion,Qt::CaseInsensitive);
    if(comp < 0)
        return false;
    else if(comp > 0)
        return true;
    else {
        //from here on we deal only with the same base framework
        if(fwDesc1.sub.isEmpty() && !fwDesc2.sub.isEmpty())
            return true;
        else if(!fwDesc1.sub.isEmpty() && fwDesc2.sub.isEmpty())
            return false;
        else if(fwDesc1.sub == fwDesc2.sub)
            return fwDesc1.develVersion > fwDesc2.develVersion;
        else
            return fwDesc1.sub > fwDesc2.sub;
    }

    //this should never be reached
    return s1 > s2;
}

UbuntuClickFrameworkProvider *UbuntuClickFrameworkProvider::m_instance = nullptr;

UbuntuClickFrameworkProvider::UbuntuClickFrameworkProvider()
    : QObject(0),
      m_policyCache {
        {QStringLiteral("ubuntu-sdk-13.10"),QStringLiteral("1.0")},
        {QStringLiteral("ubuntu-sdk-14.04"),QStringLiteral("1.1")},
        {QStringLiteral("ubuntu-sdk-14.10"),QStringLiteral("1.2")}
      },
      m_manager(nullptr),
      m_currentRequest(nullptr),
      m_cacheUpdateTimer(nullptr)
{
    Q_ASSERT_X(m_instance == nullptr,Q_FUNC_INFO,"UbuntuClickFrameworkProvider can only be instantiated once");
    m_instance = this;

    m_cacheFilePath = QStringLiteral("%1/ubuntu-sdk/framework-cache.json")
            .arg(QStandardPaths::writableLocation(QStandardPaths::ConfigLocation));

    m_manager = new QNetworkAccessManager(this);
    m_cacheUpdateTimer = new QTimer(this);
    m_cacheUpdateTimer->setInterval( 60 * 60 * 1000); //fire every hour
    m_cacheUpdateTimer->start();

    connect(m_cacheUpdateTimer,SIGNAL(timeout()),this,SLOT(updateFrameworks()));

    //read the current state
    readCache();

    //fire a update
    updateFrameworks(true);
}

UbuntuClickFrameworkProvider *UbuntuClickFrameworkProvider::instance()
{
    return m_instance;
}

QStringList UbuntuClickFrameworkProvider::supportedFrameworks() const
{
    return m_frameworkCache;
}

/*!
 * \brief UbuntuClickTool::getMostRecentFramework
 * returns the framework with the highest number supporting the subFramework
 * or a empty string of no framework with the given \a subFramework was found
 */
QString UbuntuClickFrameworkProvider::mostRecentFramework(const QString &subFramework)
{
    //cache is ordered from newest -> oldest framework
    QString currFw;
    foreach(const QString &framework, m_frameworkCache) {
        QString basename;
        QStringList extensions;

        basename = getBaseFramework(framework,&extensions);
        if(basename.isEmpty())
            continue;

        //this is a multi purpose framework
        if (extensions.size() == 0
                || (extensions.size() == 1 && extensions[0].startsWith(QLatin1String("dev")) )) {
            if (currFw.isEmpty()) {
                currFw = framework;
            }
            //if the subframework is empty we return
            //the first baseframework we can find
            if(subFramework.isEmpty())
                return currFw;
            continue;
        }

        if(extensions.contains(subFramework))
            return framework;
    }
    return currFw;
}

QString UbuntuClickFrameworkProvider::frameworkPolicy(const QString &fw) const
{
#if 0
    QProcess proc;
    proc.setProgram(QStringLiteral("aa-clickquery"));
    proc.setArguments(QStringList{
                      QStringLiteral("--click-framework=%1").arg(fw),
                      QStringLiteral("--query=policy_version")});
    proc.start();
    proc.waitForFinished();
    if(proc.exitCode() == 0 && proc.exitStatus() == QProcess::NormalExit) {
        return QString::fromUtf8(proc.readAllStandardOutput();
    }
#endif
    QString base = getBaseFramework(fw);
    if(m_policyCache.contains(base))
        return m_policyCache[base];
    return QString();
}

QStringList UbuntuClickFrameworkProvider::getSupportedFrameworks()
{
    return instance()->supportedFrameworks();
}

/*!
 * \brief UbuntuClickTool::getMostRecentFramework
 * \sa UbuntuClickFrameworkProvider::mostRecentFramework
 */
QString UbuntuClickFrameworkProvider::getMostRecentFramework(const QString &subFramework)
{
    return instance()->mostRecentFramework(subFramework);
}

QString UbuntuClickFrameworkProvider::getBaseFramework(const QString &framework, QStringList *extensions)
{
    QRegularExpression expr(QLatin1String(Constants::UBUNTU_CLICK_BASE_FRAMEWORK_REGEX));
    QRegularExpressionMatch match = expr.match(framework);
    if(match.hasMatch()) {
        QString basename = match.captured(1);
        if(extensions) {
            *extensions = QString(framework).replace(basename,
                                                     QString()).split(QChar::fromLatin1('-'),
                                                                      QString::SkipEmptyParts);
        }
        return basename;
    }
    return QString();
}

void UbuntuClickFrameworkProvider::requestFinished()
{
    //if the current request is not set anymore we already called deleteLater
    if(!m_currentRequest)
        return;

    QByteArray data = m_currentRequest->readAll();

    //make sure everything is cleaned up
    m_currentRequest->deleteLater();
    m_currentRequest = nullptr;

    //if we received nothing, stop
    if(data.isEmpty())
        return;

    //make sure we got valid data
    QStringList newData = parseData(data);
    if(newData.isEmpty())
        return;

    bool cacheIsUp2Date = (newData == m_frameworkCache);
    if(!cacheIsUp2Date) {
        QFile cache(m_cacheFilePath);
        if(cache.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            cache.write(data);
            cache.close();
        } else {
            qWarning()<<"Could not create framework cache file, falling back to default values";
        }
        m_frameworkCache = newData;
        emit frameworksUpdated();
    }
}

void UbuntuClickFrameworkProvider::requestError()
{
    //if the current request is not set anymore we already called deleteLater
    if(!m_currentRequest)
        return;

    qWarning()<<"Could not update the framework cache file. Probably there is no network connection.";

    m_currentRequest->deleteLater();
    m_currentRequest = nullptr;
}

void UbuntuClickFrameworkProvider::updateFrameworks(bool force)
{
    if(m_currentRequest)
        return;

    if(!force) {
        //update every 12 hours
        QFileInfo info(m_cacheFilePath);
        if(info.exists() && info.lastModified().secsTo(QDateTime::currentDateTime()) < (12*60*60))
            return;
    }

    //fire the request
    m_currentRequest = m_manager->get(QNetworkRequest(QUrl(QStringLiteral("https://myapps.developer.ubuntu.com/dev/api/click-framework/"))));
    connect(m_currentRequest,SIGNAL(finished()),this,SLOT(requestFinished()));
    connect(m_currentRequest,SIGNAL(error(QNetworkReply::NetworkError)),this,SLOT(requestError()));
}

void UbuntuClickFrameworkProvider::readCache()
{
    QFile cache(m_cacheFilePath);
    if(!cache.exists() || !cache.open(QIODevice::ReadOnly)) {
        readDefaultValues();
        return;
    }

    QStringList data = parseData(cache.readAll());
    if(!data.isEmpty()) {
        m_frameworkCache = data;
        emit frameworksUpdated();
    } else {
        //if the cache is empty fall back to the default values
        if(m_frameworkCache.isEmpty())
            readDefaultValues();
    }
}

void UbuntuClickFrameworkProvider::readDefaultValues()
{
    QFile cache(QStringLiteral(":/ubuntu/click-framework.json"));
    if(Q_UNLIKELY(cache.open(QIODevice::ReadOnly) == false)) {
        //This codepath is very unlikely, but lets still make sure there is a message to the user
        qWarning()<<"Could not read cache file OR default values. No frameworks are available to select from";
        return;
    }

    QStringList data = parseData(cache.readAll());
    if(!data.isEmpty()) {
        m_frameworkCache = data;
        emit frameworksUpdated();
    }
}

QStringList UbuntuClickFrameworkProvider::parseData(const QByteArray &data) const
{
    QJsonParseError parseError;
    parseError.error = QJsonParseError::NoError;

    QJsonDocument doc = QJsonDocument::fromJson(data,&parseError);
    if(parseError.error != QJsonParseError::NoError) {
        qWarning()<< "Could not parse the framework cache: "
                  << parseError.errorString();
        return QStringList();
    }

    QJsonObject obj   = doc.object();
    QStringList result;

    for(auto i = obj.constBegin(); i != obj.constEnd(); i++ ) {
        if(!i.key().startsWith(QStringLiteral("ubuntu-sdk")))
            continue;

        if(i.value().toString() == QStringLiteral("available")) {
            result += i.key();
        }
    }

    qSort(result.begin(),result.end(),caseInsensitiveFWLessThan);
    return result;
}

} // namespace Internal
} // namespace Ubuntu

