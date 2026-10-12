// Diz se a área de trabalho desta tela está coberta por uma janela
// maximizada ou em tela cheia (ou seja, o widget não está visível).

import QtQuick
import org.kde.taskmanager as TaskManager

Item {
    id: root

    property rect screenGeometry
    property bool covered: false

    function update() {
        let found = false;
        for (let i = 0; i < tasks.count && !found; ++i) {
            const index = tasks.index(i, 0);
            found = tasks.data(index, TaskManager.AbstractTasksModel.IsWindow)
                && (tasks.data(index, TaskManager.AbstractTasksModel.IsMaximized)
                    || tasks.data(index, TaskManager.AbstractTasksModel.IsFullScreen));
        }
        covered = found;
    }

    TaskManager.VirtualDesktopInfo {
        id: desktopInfo
    }

    TaskManager.ActivityInfo {
        id: activityInfo
    }

    TaskManager.TasksModel {
        id: tasks

        groupMode: TaskManager.TasksModel.GroupDisabled
        virtualDesktop: desktopInfo.currentDesktop
        activity: activityInfo.currentActivity
        screenGeometry: root.screenGeometry
        filterByVirtualDesktop: true
        filterByActivity: true
        filterByScreen: true
        filterMinimized: true

        onDataChanged: Qt.callLater(root.update)
        onCountChanged: Qt.callLater(root.update)
        onModelReset: Qt.callLater(root.update)
        Component.onCompleted: root.update()
    }
}
