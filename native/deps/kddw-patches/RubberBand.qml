/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2020 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  LOBS patch: the landing preview, a soft blue rounded area showing exactly where the
  panel will go when you let go.
*/

import QtQuick 2.9

Rectangle {
    id: root
    anchors.fill: parent
    radius: 10
    color: "#336b97ff"
    border.color: "#6b97ff"
    border.width: 2
}
