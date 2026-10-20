/*
  This file is part of KDDockWidgets.

  SPDX-FileCopyrightText: 2020 Klarälvdalens Datakonsult AB, a KDAB Group company <info@kdab.com>
  Author: Sergio Martins <sergio.martins@kdab.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only

  LOBS patch: drawn drop targets (rounded tile + a mini diagram of where the panel lands)
  instead of the stock PNG arrows. Colors match LOBS's Fluid Functionalism dark tokens.
*/

import QtQuick 2.9
import com.kdab.dockwidgets 2.0

Rectangle {
    id: root
    required property ClassicDropIndicatorOverlay overlayWindow
    property int indicatorType: KDDockWidgets.DropLocation_None
    readonly property bool isHovered: root.overlayWindow && root.overlayWindow.currentDropLocation === indicatorType

    readonly property bool isLeft: indicatorType === KDDockWidgets.DropLocation_Left || indicatorType === KDDockWidgets.DropLocation_OutterLeft
    readonly property bool isRight: indicatorType === KDDockWidgets.DropLocation_Right || indicatorType === KDDockWidgets.DropLocation_OutterRight
    readonly property bool isTop: indicatorType === KDDockWidgets.DropLocation_Top || indicatorType === KDDockWidgets.DropLocation_OutterTop
    readonly property bool isBottom: indicatorType === KDDockWidgets.DropLocation_Bottom || indicatorType === KDDockWidgets.DropLocation_OutterBottom
    readonly property bool isCenter: indicatorType === KDDockWidgets.DropLocation_Center

    width: 40
    height: 40
    radius: 10
    color: isHovered ? "#6b97ff" : "#2c2c2c"
    border.width: 1
    border.color: isHovered ? "#8fb0ff" : "#33ffffff"
    scale: isHovered ? 1.08 : 1
    Behavior on color { ColorAnimation { duration: 80 } }
    Behavior on scale { NumberAnimation { duration: 80; easing.type: Easing.OutCubic } }

    // Mini panel diagram: the outline is the target, the filled part is where the panel goes.
    Rectangle {
        id: frame
        anchors.centerIn: parent
        width: 22
        height: 22
        radius: 4
        color: "transparent"
        border.width: 1.5
        border.color: root.isHovered ? "white" : "#a3a3a3"
        Rectangle {
            radius: 2
            color: root.isHovered ? "white" : "#a3a3a3"
            x: root.isRight ? parent.width / 2 : 3
            y: root.isBottom ? parent.height / 2 : 3
            width: (root.isLeft || root.isRight) ? parent.width / 2 - 3 : parent.width - 6
            height: (root.isTop || root.isBottom) ? parent.height / 2 - 3 : parent.height - 6
        }
    }
    // Outer (window-edge) targets get a subtle double outline so they read as "whole side".
    Rectangle {
        visible: root.indicatorType >= KDDockWidgets.DropLocation_OutterLeft
        anchors.fill: parent
        anchors.margins: -4
        radius: 13
        color: "transparent"
        border.width: 1
        border.color: "#1fffffff"
    }
}
