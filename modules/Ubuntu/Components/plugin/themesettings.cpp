/*
 * Copyright 2012 Canonical Ltd.
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
 * Author: Zsombor Egri <zsombor.egri@canonical.com>
 */

#include "themeengine.h"
#include "themeengine_p.h"
#include "style.h"
#include "styleditem.h"

#include <QtCore/QFileInfo>
#include <QtCore/QDir>

/*!
  \internal
  ThemeSettings class handles the selection of the application style based on
  global and application settings.

  User theme settings are stored in $HOME/.qthm/theme.ini file, which contains
  the current global theme name and the QML import paths. These settings are
  stored in "theme" and "imports" keywords.

  System themes are stored under /usr/share/themes/<theme-name>/qthm folder,
  where the common theme is named "default.qthm" and each application has its
  own theme, which can be either in the qthm folder or under a subfolder.

  OBS: we need to expose a QML element that handles these. Beside, applications
  should set up the theme before the plugin gets loaded by QML.
  */


// qmlviewer/qmlscene stores its settings in $HOME/.config/Nokia/QtQmlViewer.conf
// therefore it is possible to have application specific theme settings
// for it!

// user's theme settings are stored in ~/.qthm/theme.ini file and here are
// also stored the user specific themes
const char *PathFormat_GlobalThemeIniFile = "%1/.qthm/theme.ini";

#ifdef TARGET_DEMO
// for SDK demo we use the demo folder to store both global and private themes
const char *PathFormat_GlobalAppTheme = "demos/%1/%2";
const char *PathFormat_GlobalThemeFile = "demos/%1/default.qthm";
const char *systemThemePath = "demos";

#else

const char *PathFormat_GlobalAppTheme = "/usr/share/themes/%1/qthm/%2";
const char *PathFormat_GlobalThemeFile = "/usr/share/themes/%1/qthm/default.qthm";
const char *systemThemePath = "/usr/share/themes";

#endif

const char *globalThemeKey = "theme";
const char *importPathsKey = "imports";
const char *appUseGlobalThemeKey = "UseSystemTheme";
const char *appThemeFileKey = "ThemeFile";


/*!
  \internal
  Instanciates the settins and connects the file system watcher
  */
ThemeSettings::ThemeSettings(QObject *parent) :
    configWatcher(parent),
    globalSettings(QString(PathFormat_GlobalThemeIniFile).arg(QDir::homePath()), QSettings::IniFormat),
    hasAppSettings(false),
    hasGlobalSettings(false)
{
    hasAppSettings = (QFile::exists(appSettings.fileName()) &&
            (appSettings.contains(appUseGlobalThemeKey) &&
             appSettings.contains(appThemeFileKey)));

    // check if we have system settings file, if not, create one
    if (!QFile::exists(globalSettings.fileName())) {
        // create the file by writing the default theme
#ifdef TARGET_DEMO
        globalSettings.setValue(globalThemeKey, "global-themes");
#else
        // TODO: figure out how to get the default theme for the release
        globalSettings.setValue(globalThemeKey, "Ambiance");
#endif
    }

    hasGlobalSettings = QFile::exists(globalSettings.fileName()) &&
            (globalSettings.status() == QSettings::NoError);

    if ((hasAppSettings && appSettings.value(appUseGlobalThemeKey).toBool()) || hasGlobalSettings)
        configWatcher.addPath(globalSettings.fileName());

    QObject::connect(&configWatcher, SIGNAL(fileChanged(QString)), parent, SLOT(_q_updateTheme()));
}

/*!
  \internal
  Returns the application's theme file as defined either in application or global
  theme settings file.
  An application can decide whether to use the global's default theme, the application
  specific theme defined by the global one or a private theme. To support this,
  applications must have the setting keys specified in \a appUseGlobalThemeKey and
  \a appThemeFileKey. If these are not defined, the global default theme will be loaded.
  */
QUrl ThemeSettings::themeFile() const
{
    QUrl result;
    if (hasAppSettings) {
        bool useGlobalTheme = appSettings.value(appUseGlobalThemeKey).toBool();
        if (useGlobalTheme) {
            QString appTheme = appSettings.value(appThemeFileKey).toString();
            if (!appTheme.isEmpty()) {
                // build theme file so it is taken from the global area
                result = QUrl::fromLocalFile(QString(PathFormat_GlobalAppTheme)
                               .arg(globalSettings.value(globalThemeKey).toString())
                               .arg(appTheme));
            }
        } else {
            // the theme specified in settings is a private one not dependent directly to
            // the global themes
            result = QUrl::fromLocalFile(appSettings.value(appThemeFileKey).toString());
        }
    }

    // check if the application succeeded to set the URL, and if not, use the global defined
    // theme if possible
    if (hasGlobalSettings && (!result.isValid() || result.path().isEmpty())) {
        QString theme = globalSettings.value(globalThemeKey).toString();
        result = QUrl::fromLocalFile(QString(PathFormat_GlobalThemeFile).arg(theme));
        if (!QFile::exists(result.path()))
            result = QUrl();
    }

    return result;
}
/*!
  \internal
  This method sets the theme file for an application. It does not alter the global theme
  defined for the user. In case the application doesn't have theme settings set yet, it
  will try to create those aswell. Returns the URL (full path) to the theme file. The
  application will use the default theme setup if the \a url is invalid.
  */
QUrl ThemeSettings::setTheme(const QString &theme, bool global)
{
    if (theme.isEmpty() && !global) {
        ThemeEnginePrivate::setError("Invalid private file given for theme!");
        return QUrl();
    }

    if (!hasAppSettings && QFile::exists(appSettings.fileName())) {
        // application is configured to accept settings, so force app settings
        hasAppSettings = true;
    }

    if (hasAppSettings) {

        // check whether the setting can be applied, if not, report error
        bool prevGlobal = appSettings.value(appUseGlobalThemeKey).toBool();
        QString prevTheme = appSettings.value(appThemeFileKey).toString();

        appSettings.setValue(appUseGlobalThemeKey, global);
        appSettings.setValue(appThemeFileKey, theme);
        QUrl theme = themeFile();
        if (!theme.isValid() || !QFile::exists(theme.path())) {
            // roll back settings
            appSettings.setValue(appUseGlobalThemeKey, prevGlobal);
            appSettings.setValue(appThemeFileKey, prevTheme);
            ThemeEnginePrivate::setError("Invalid theme setting!");
            return QUrl();
        }

        if ((hasAppSettings && !global) || hasGlobalSettings)
            configWatcher.removePath(globalSettings.fileName());

        return theme;
    }
    return QUrl();
}

/*!
  \internal
  Returns the QML import paths supporting the defined theme file.
  */
QStringList ThemeSettings::imports() const
{
    QStringList result;
    QString imports;

    if (hasGlobalSettings && appSettings.value(appUseGlobalThemeKey).toBool()) {
        imports = globalSettings.value(importPathsKey).toString();
        if (!imports.isEmpty())
            result += imports.split(',');
    }
    if (hasAppSettings) {
        imports = appSettings.value(importPathsKey).toString();
        if (!imports.isEmpty())
            result += imports.split(',');
    }

    return result;
}
