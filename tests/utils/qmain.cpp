/*
 * Copyright © 2014 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *     Pete Woods <pete.woods@canonical.com>
 */

//#include <config.h>

#include <QCoreApplication>
#include <QTimer>
#include <gtest/gtest.h>

#include <libqtdbusmock/DBusMock.h>

using namespace QtDBusMock;

class Runner: public QObject
{
    Q_OBJECT
public Q_SLOTS:
    void run()
    {
        QCoreApplication::exit(RUN_ALL_TESTS());
    }
};

int main(int argc, char **argv)
{
    qputenv("LANG", "C.UTF-8");
    unsetenv("LC_ALL");

    // boilerplate i18n
    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, GNOMELOCALEDIR);
    textdomain(GETTEXT_PACKAGE);

    QCoreApplication application(argc, argv);
    DBusMock::registerMetaTypes();
    ::testing::InitGoogleTest(&argc, argv);

    Runner runner;
    QTimer::singleShot(0, &runner, SLOT(run()));

    return application.exec();
}

#include "qmain.moc"
