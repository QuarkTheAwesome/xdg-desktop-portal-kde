/*
 * Copyright © 2018 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Jan Grulich <jgrulich@redhat.com>
 */

#ifndef XDG_DESKTOP_PORTAL_KDE_SCREENCHOOSER_DIALOG_H
#define XDG_DESKTOP_PORTAL_KDE_SCREENCHOOSER_DIALOG_H

#include <QAbstractListModel>
#include <QDialog>

namespace Ui
{
class ScreenChooserDialog;
}

class ScreenCastPortalOutput;

class ScreenChooserDialog : public QDialog
{
    Q_OBJECT
public:
    ScreenChooserDialog(const QMap<quint32, ScreenCastPortalOutput> &screens, bool multiple = false, QDialog *parent = nullptr, Qt::WindowFlags flags = 0);
    ~ScreenChooserDialog();

    QList<quint32> selectedScreens() const;

private:
    Ui::ScreenChooserDialog * m_dialog;
};

#endif // XDG_DESKTOP_PORTAL_KDE_SCREENCHOOSER_DIALOG_H