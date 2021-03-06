
/* BEGIN_COMMON_COPYRIGHT_HEADER
 *
 * TOra - An Oracle Toolkit for DBA's and developers
 *
 * Shared/mixed copyright is held throughout files in this product
 *
 * Portions Copyright (C) 2000-2001 Underscore AB
 * Portions Copyright (C) 2003-2005 Quest Software, Inc.
 * Portions Copyright (C) 2004-2013 Numerous Other Contributors
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
 * along with this program as the file COPYING.txt; if not, please see
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt.
 *
 *      As a special exception, you have permission to link this program
 *      with the Oracle Client libraries and distribute executables, as long
 *      as you follow the requirements of the GNU GPL in regard to all of the
 *      software in the executable aside from Oracle client libraries.
 *
 * All trademarks belong to their respective owners.
 *
 * END_COMMON_COPYRIGHT_HEADER */

#include "tools/toscriptschemawidget.h"
#include "tools/toscripttreemodel.h"
#include "tools/toscripttreeitem.h"
#include "core/utils.h"
#include "core/tosql.h"
#include "core/toquery.h"
#include "core/toconnectionregistry.h"
#include "core/toglobalevent.h"

static toSQL SQLSchemasMySQL("toScriptSchemaWidget:ExtractSchema",
                             "SHOW DATABASES",
                             "Get usernames available in database, must have same columns",
                             "3.23",
                             "MySQL");

static toSQL SQLSchemas("toScriptSchemaWidget:ExtractSchema",
                        "SELECT username FROM sys.all_users ORDER BY username",
                        "");


toScriptSchemaWidget::toScriptSchemaWidget(QWidget * parent)
    : QWidget(parent)
{
    setupUi(this);
    WorkingWidget->hide();
    WorkingWidget->setType(toWorkingWidget::NonInteractive);

    Model = new toScriptTreeModel(this);
    ObjectsView->setModel(Model);

    ConnectionComboBox->setModel(&toConnectionRegistrySing::Instance());

    connect(ConnectionComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(changeConnection(int)));
    connect(SchemaComboBox, SIGNAL(currentIndexChanged(int)),
            this, SLOT(changeSchema(int)));

    connect(ObjectsView->selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
            this,
            SLOT(objectsView_selectionChanged(const QItemSelection &, const QItemSelection &)));
};

void toScriptSchemaWidget::setTitle(const QString & text)
{
    GroupBox->setTitle(text);
}

void toScriptSchemaWidget::changeConnection(int val)
{
//     TLOG(2,toDecorator,__HERE__) << "toScriptSchemaWidget::changeConnection" << val;
    if (val == -1)
        return;

    SchemaComboBox->blockSignals(true);

    SchemaComboBox->clear();
    toConnection &conn = toConnectionRegistrySing::Instance().connection(this->connectionOptions());
    toQList schema = toQuery::readQuery(conn, SQLSchemas, toQueryParams());
    SchemaComboBox->addItem(tr("All"));
    while (schema.size() > 0)
        SchemaComboBox->addItem((QString)Utils::toShift(schema));

    int ix = SchemaComboBox->findText(conn.user().toUpper(), Qt::MatchExactly);
    if (ix == -1)
        ix = 0;
    SchemaComboBox->blockSignals(false);
    SchemaComboBox->setCurrentIndex(ix);
}

toConnectionOptions toScriptSchemaWidget::connectionOptions()
{
    QModelIndex i = ConnectionComboBox->model()->index(ConnectionComboBox->currentIndex(), 0);
    QVariant data = ConnectionComboBox->model()->data(i, Qt::UserRole);
    return data.value<toConnectionOptions>();
}

toConnection& toScriptSchemaWidget::connection()
{
    return toConnectionRegistrySing::Instance().connection(connectionOptions());
}

void toScriptSchemaWidget::setConnectionString(const QString & c)
{
    int i = ConnectionComboBox->findText(c);
    if (i == -1)
        return;
    ConnectionComboBox->setCurrentIndex(i);
    changeConnection(i);
}

//void toScriptSchemaWidget::delConnection(const QString &name)
//{
//    ConnectionComboBox->removeItem(ConnectionComboBox->findText(name));
//}

//void toScriptSchemaWidget::addConnection(const QString & name)
//{
//    ConnectionComboBox->addItem(name);
//}

void toScriptSchemaWidget::changeSchema(int val)
{
//     TLOG(2,toDecorator,__HERE__) << "toScriptSchemaWidget::changeSchema" << val;
    if (val == -1)
        return;
    QString schema;
    if (val != 0)
        schema = SchemaComboBox->itemText(val);

    setEnabled(false);
    WorkingWidget->show();
    QCoreApplication::processEvents();
    Model->setupModelData(this->connectionOptions(), schema);
    WorkingWidget->hide();
    setEnabled(true);
}

QItemSelectionModel * toScriptSchemaWidget::objectList()
{
    return ObjectsView->selectionModel();
}

void toScriptSchemaWidget::objectsView_selectionChanged(const QItemSelection & selected,
        const QItemSelection & deselected)
{
    // all other widgets are disabled until it ends
    setEnabled(false);
    WorkingWidget->show();

    foreach (QModelIndex i, selected.indexes())
    subSelectionChanged(i);

    WorkingWidget->hide();
    setEnabled(true);
}

void toScriptSchemaWidget::subSelectionChanged(QModelIndex ix)
{
    QCoreApplication::processEvents();

    QItemSelectionModel * sel = ObjectsView->selectionModel();
    toScriptTreeItem * item = static_cast<toScriptTreeItem*>(ix.internalPointer());
    toScriptTreeItem * subItem;
    QModelIndex subIx;

    for (int i = 0; i < item->childCount(); ++i)
    {
        subIx = Model->index(i, 0, ix);
        subItem = static_cast<toScriptTreeItem*>(subIx.internalPointer());

        if (!sel->isSelected(subIx))
            sel->select(subIx, QItemSelectionModel::Select);

        if (subItem->childCount() > 0)
            subSelectionChanged(subIx);
    }
}

