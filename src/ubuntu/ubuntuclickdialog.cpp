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
#include "ubuntuclickdialog.h"
#include "ui_ubuntuclickdialog.h"
#include "ubuntuconstants.h"
#include "clicktoolchain.h"
#include "ubuntukitmanager.h"

#include <QMessageBox>
#include <QPushButton>

#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/processparameters.h>
#include <projectexplorer/toolchainmanager.h>
#include <texteditor/fontsettings.h>


#include "ubuntucreatenewchrootdialog.h"

namespace Ubuntu {
namespace Internal {

UbuntuClickDialog::UbuntuClickDialog(QWidget *parent)
    : QDialog(parent)
    ,m_ui(new Ui::UbuntuClickDialog)
{
    m_ui->setupUi(this);

    QFont f(TextEditor::FontSettings::defaultFixedFontFamily());
    f.setStyleHint(QFont::TypeWriter);
    m_ui->output->setFont(f);
    m_ui->exitStatusLabel->setVisible(false);

    m_process = new Utils::QtcProcess(this);
    connect(m_process,SIGNAL(readyReadStandardOutput()),this,SLOT(on_clickReadyReadStandardOutput()));
    connect(m_process,SIGNAL(readyReadStandardError()),this,SLOT(on_clickReadyReadStandardError()));
    connect(m_process,SIGNAL(finished(int)),this,SLOT(on_clickFinished(int)));
}

UbuntuClickDialog::~UbuntuClickDialog()
{
    delete m_ui;
}

void UbuntuClickDialog::setParameters(ProjectExplorer::ProcessParameters *params)
{
    params->resolveAll();
    m_process->setCommand(params->command(),params->arguments());
    m_process->setEnvironment(params->environment());
    m_process->setWorkingDirectory(params->workingDirectory());
}

int UbuntuClickDialog::lastExitCode() const
{
    return m_exitCode;
}

void UbuntuClickDialog::runClick( )
{
#if 0
    //change the button to cancel
    m_buttonBox->clear();
    m_buttonBox->addButton(QDialogButtonBox::Cancel);
#endif
    disableCloseButton(true);
    m_process->start();
}

int UbuntuClickDialog::runClickModal(ProjectExplorer::ProcessParameters *params)
{
    UbuntuClickDialog dlg(Core::ICore::mainWindow());
    dlg.setParameters(params);
    QMetaObject::invokeMethod(&dlg,"runClick",Qt::QueuedConnection);
    dlg.exec();

    return dlg.m_exitCode;
}

bool UbuntuClickDialog::createClickChrootModal(bool redetectKits, const QString &arch)
{

    UbuntuClickTool::Target t;
    if(!UbuntuCreateNewChrootDialog::getNewChrootTarget(&t,arch))
        return false;

    ProjectExplorer::ProcessParameters params;
    UbuntuClickTool::parametersForCreateChroot(t,&params);

    bool success = (runClickModal(&params) == 0);

    if(success) {
        ClickToolChain* tc = new ClickToolChain(t, ProjectExplorer::ToolChain::AutoDetection);
        ProjectExplorer::ToolChainManager::registerToolChain(tc);

        if(redetectKits)
            UbuntuKitManager::autoDetectKits();
    }

    return success;
}

int UbuntuClickDialog::maintainClickModal(const UbuntuClickTool::Target &target, const UbuntuClickTool::MaintainMode &mode)
{
    if(mode == UbuntuClickTool::Delete) {
        QString title = tr(Constants::UBUNTU_CLICK_DELETE_TITLE);
        QString text  = tr(Constants::UBUNTU_CLICK_DELETE_MESSAGE);
        if( QMessageBox::question(Core::ICore::mainWindow(),title,text) != QMessageBox::Yes )
            return 0;
    }

    ProjectExplorer::ProcessParameters params;
    UbuntuClickTool::parametersForMaintainChroot(mode,target,&params);
    return runClickModal(&params);
}

void UbuntuClickDialog::done(int code)
{
    if(code == QDialog::Rejected) {
        if(m_process->state() != QProcess::NotRunning) {
            //ask the user if he really wants to do that
            QString title = tr(Constants::UBUNTU_CLICK_STOP_TITLE);
            QString text  = tr(Constants::UBUNTU_CLICK_STOP_MESSAGE);
            if( QMessageBox::question(Core::ICore::mainWindow(),title,text)!= QMessageBox::Yes )
                return;

            m_process->terminate();
            m_process->waitForFinished(100);
            m_process->kill();

            m_ui->exitStatusLabel->setText(tr(Constants::UBUNTU_CLICK_STOP_WAIT_MESSAGE));
            m_ui->exitStatusLabel->setVisible(true);
            return;
        }
    }
    QDialog::done(code);
}

void UbuntuClickDialog::disableCloseButton(const bool &disabled)
{
    QPushButton* bt = m_ui->buttonBox->button(QDialogButtonBox::Close);
    if(bt) bt->setDisabled(disabled);
}

void UbuntuClickDialog::on_clickFinished(int exitCode)
{
    disableCloseButton(false);
#if 0
    //set the button to close again
    m_buttonBox->clear();
    m_buttonBox->addButton(QDialogButtonBox::Close);
#endif

    if (exitCode != 0) {
        on_clickReadyReadStandardError(tr("---%0---").arg(QLatin1String(Constants::UBUNTU_CLICK_ERROR_EXIT_MESSAGE)));
    } else {
        on_clickReadyReadStandardOutput(tr("---%0---").arg(QLatin1String(Constants::UBUNTU_CLICK_SUCCESS_EXIT_MESSAGE)));
    }

    m_exitCode = exitCode;
}

void UbuntuClickDialog::on_clickReadyReadStandardOutput(const QString txt)
{
    QTextCursor cursor(m_ui->output->document());
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat tf;

    QFont font = m_ui->output->font();
    tf.setFont(font);
    tf.setForeground(m_ui->output->palette().color(QPalette::Text));

    if(txt.isEmpty())
        cursor.insertText(QString::fromLocal8Bit(m_process->readAllStandardOutput()), tf);
    else
        cursor.insertText(txt, tf);
}

void UbuntuClickDialog::on_clickReadyReadStandardError(const QString txt)
{
    QTextCursor cursor(m_ui->output->document());
    QTextCharFormat tf;

    QFont font = m_ui->output->font();
    QFont boldFont = font;
    boldFont.setBold(true);
    tf.setFont(boldFont);
    tf.setForeground(QColor(Qt::red));

    if(txt.isEmpty())
        cursor.insertText(QString::fromLocal8Bit(m_process->readAllStandardError()), tf);
    else
        cursor.insertText(txt, tf);
}

} // namespace Internal
} // namespace Ubuntu
