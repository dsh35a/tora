/*****
*
* TOra - An Oracle Toolkit for DBA's and developers
* Copyright (C) 2003-2005 Quest Software, Inc
* Portions Copyright (C) 2005 Other Contributors
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation;  only version 2 of
* the License is valid for this program.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*
*      As a special exception, you have permission to link this program
*      with the Oracle Client libraries and distribute executables, as long
*      as you follow the requirements of the GNU GPL in regard to all of the
*      software in the executable aside from Oracle client libraries.
*
*      Specifically you are not permitted to link this program with the
*      Qt/UNIX, Qt/Windows or Qt Non Commercial products of TrollTech.
*      And you are not permitted to distribute binaries compiled against
*      these libraries without written consent from Quest Software, Inc.
*      Observe that this does not disallow linking to the Qt Free Edition.
*
*      You may link this product with any GPL'd Qt library such as Qt/Free
*
* All trademarks belong to their respective owners.
*
*****/


#include "utils.h"

#include "toabout.h"
#include "toconf.h"
#include "toconnection.h"
#include "tohighlightedtext.h"
#include "tomain.h"
#include "tosql.h"
#include "totool.h"

// qt4 in via the qtranslator
// #include "tora_toad.h"

#ifndef Q_OS_WIN32
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <memory>

#include <qapplication.h>
#include <qmessagebox.h>
#include <qtextcodec.h>
//Added by qt3to4:
#include <QString>
#include <QTranslator>

#ifndef TOMONOLITHIC
#include <dlfcn.h>


#include <tosplash.h>
#include <qdir.h>
#include <qfileinfo.h>
#include <qlabel.h>
#include <QProgressBar>
#endif

bool toMonolithic(void)
{
#ifdef TOMONOLITHIC
    return true;
#else

    return false;
#endif
}

void toUpdateIndicateEmpty(void);

int main(int argc, char **argv)
{
#ifdef ENABLE_QT_XFT
    toSetEnv("QT_XFT", toConfigurationSingle::Instance().globalConfig(CONF_QT_XFT, DEFAULT_QT_XFT).latin1());
#endif

#  ifndef Q_OS_WIN32

    if (toConfigurationSingle::Instance().globalConfig(CONF_DESKTOP_AWARE, "Yes").isEmpty())
        QApplication::setDesktopSettingsAware(false);
#  endif

    new QApplication(argc, argv);

    try
    {

// qt4        if (getenv("LANG"))
//             qApp->setDefaultCodec(QTextCodec::codecForName(getenv("LANG")));

        QTranslator torats(0);
        torats.load(toPluginPath() + "/" + QString("tora_") + toConfigurationSingle::Instance().globalConfig(CONF_LOCALE, QTextCodec::locale()), ".");
        qApp->installTranslator(&torats);
        QTranslator toadbindings(0);
        if (!toConfigurationSingle::Instance().globalConfig(CONF_TOAD_BINDINGS, DEFAULT_TOAD_BINDINGS).isEmpty())
        {
            // qt4 - hot candidate for a builtin resource
//             if (!toadbindings.load(tora_toad, sizeof(tora_toad)))
//                 printf("Internal error, couldn't load TOAD bindings");
            toadbindings.load(toPluginPath() + "/" + "tora_toad.qm");
            qApp->installTranslator(&toadbindings);
        }

#ifdef ENABLE_STYLE
        QString style = toConfigurationSingle::Instance().globalConfig(CONF_STYLE, "");
        if (!style.isEmpty())
            toSetSessionType(style);
#endif

#ifndef TOMONOLITHIC

        {
            toSplash splash(NULL, "About " TOAPPNAME, false);
            splash.show();
            std::list<QString> failed;
            QString dirPath = toPluginPath();
            QDir d(dirPath, QString::fromLatin1("*.tso"), QDir::Name, QDir::Files);
            if (!d.exists())
            {
                fprintf(stderr,
                        "Couldn't find PluginDir, falling back to default: %s\n",
                        DEFAULT_PLUGIN_DIR);
                dirPath = DEFAULT_PLUGIN_DIR;
                d.cd(dirPath);
                if (d.exists())
                    toConfigurationSingle::Instance().globalSetConfig(CONF_PLUGIN_DIR, dirPath);
                else
                    fprintf(stderr, "Invalid PluginDir.\n");
            }

            for (unsigned int i = 0;i < d.count();i++)
            {
                failed.insert(failed.end(), d.filePath(d[i]));
            }
            QProgressBar *progress = splash.progress();
            QLabel *label = splash.label();
            progress->setTotalSteps(failed.size());
            progress->setProgress(1);
            qApp->processEvents();
            bool success;
            do
            {
                success = false;
                std::list<QString> current = failed;
                failed.clear();
                for (std::list<QString>::iterator i = current.begin();i != current.end();i++)
                {
                    if (!dlopen(*i, RTLD_NOW | RTLD_GLOBAL))
                    {
                        failed.insert(failed.end(), *i);
                    }
                    else
                    {
                        success = true;
                        progress->setProgress(progress->progress() + 1);
                        QFileInfo file(*i);
                        label->setText(qApp->translate("main", "Loaded plugin %1").arg(file.fileName()));
                        qApp->processEvents();
                    }
                }
            }
            while (failed.begin() != failed.end() && success);

            for (std::list<QString>::iterator i = failed.begin();i != failed.end();i++)
                if (!dlopen(*i, RTLD_NOW | RTLD_GLOBAL))
                    fprintf(stderr, "Failed to load %s\n  %s\n",
                            (const char *)(*i), dlerror());
        }
#endif

        try
        {
            toSQL::loadSQL(toConfigurationSingle::Instance().globalConfig(CONF_SQL_FILE, DEFAULT_SQL_FILE));
        }
        catch (...)
            {}
        toConnectionProvider::initializeAll();

        {
            QString nls = getenv("NLS_LANG");
            if (nls.isEmpty())
                nls = "american_america.UTF8";
            else
            {
                int pos = nls.findRev('.');
                if (pos > 0)
                    nls = nls.left(pos);
                nls += ".UTF8";
            }
            toSetEnv("NLS_LANG", nls);
        }

        if (toConfigurationSingle::Instance().globalConfig("LastVersion", "") != TOVERSION)
        {
            std::auto_ptr<toAbout> about ( new toAbout(toAbout::About, NULL, "About " TOAPPNAME, true));
            if (!about->exec())
            {
                exit (2);
            }
            toConfigurationSingle::Instance().globalSetConfig("LastVersion", TOVERSION);
        }


        if (toConfigurationSingle::Instance().globalConfig("FirstInstall", "").isEmpty())
        {
            time_t t;
            time(&t);
            toConfigurationSingle::Instance().globalSetConfig("FirstInstall", ctime(&t));
        }

        toQValue::setNumberFormat(
            toConfigurationSingle::Instance().globalConfig(CONF_NUMBER_FORMAT, DEFAULT_NUMBER_FORMAT).toInt(),
            toConfigurationSingle::Instance().globalConfig(CONF_NUMBER_DECIMALS, DEFAULT_NUMBER_DECIMALS).toInt());

        if (qApp->argc() > 2 || (qApp->argc() == 2 && qApp->argv()[1][0] == '-'))
        {
            printf("Usage:\n\n  tora [{X options}] [connectstring]\n\n");
            exit(2);
        }
        else if (qApp->argc() == 2)
        {
            QString connect = QString::fromLatin1(qApp->argv()[1]);
            QString user;
            int pos = connect.find(QString::fromLatin1("@"));
            if (pos > -1)
            {
                user = connect.left(pos);
                connect = connect.right(connect.length() - pos - 1);
            }
            else
            {
                user = connect;
                if (getenv("ORACLE_SID"))
                    connect = QString::fromLatin1(getenv("ORACLE_SID"));
            }
            if (!connect.isEmpty())
                toConfigurationSingle::Instance().globalSetConfig(CONF_DATABASE, connect);
            pos = user.find(QString::fromLatin1("/"));
            if (pos > -1)
            {
                toConfigurationSingle::Instance().globalSetConfig(CONF_PASSWORD, user.right(user.length() - pos - 1));
                user = user.left(pos);
            }
            if (!user.isEmpty())
                toConfigurationSingle::Instance().globalSetConfig(CONF_USER, user);
        }

        toMarkedText::setDefaultTabWidth(
            toConfigurationSingle::Instance().globalConfig(CONF_TAB_STOP, DEFAULT_TAB_STOP).toInt());

        toUpdateIndicateEmpty();



        new toMain;

        int ret = qApp->exec();
        return ret;
    }
    catch (const QString &str)
    {
        fprintf(stderr, "Unhandled exception:\n\n%s\n",
                (const char *)str);
        TOMessageBox::critical(NULL,
                               qApp->translate("main", "Unhandled exception"),
                               str,
                               qApp->translate("main", "Exit"));
    }
    return 1;
}