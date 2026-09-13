import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI

// 所属电站多选筛选(电桩管理页):
// 触发器显示 摘要文本;下拉面板含 站内搜索、"最近管理"推荐区(本机真实记录,
// 来自控制器 recentStations)、全量电站勾选列表(超长折叠,可展开)与底部操作。
// 组件不持有选择状态:选中变化通过 selectionChanged(ids) 上交页面驱动查询。
Button {
    id: control
    property var options: []        // 全量电站 [{id,name,pile_count}]
    property var recentOptions: []  // 最近管理 [{id,name,pile_count}]
    property var selectedIds: []    // 当前选中电站 id(由页面赋值)
    property string placeholder: "所属电站：全部"
    signal selectionChanged(var ids)

    property int collapsedCount: 8  // 列表折叠阈值,超出后需"展开全部"
    property bool expanded: false
    property string searchText: ""

    function matchesSearch(name) {
        return searchText === "" || String(name).toLowerCase().indexOf(searchText.toLowerCase()) >= 0
    }

    function isChecked(id) {
        var sel = selectedIds
        for (var i = 0; i < sel.length; i++) if (Number(sel[i]) === Number(id)) return true
        return false
    }

    function toggleId(id) {
        var next = []
        var found = false
        for (var i = 0; i < selectedIds.length; i++) {
            if (Number(selectedIds[i]) === Number(id)) found = true
            else next.push(selectedIds[i])
        }
        if (!found) next.push(id)
        selectionChanged(next)
    }

    // 触发器摘要:未选=全部;单选显示站名;多选显示数量
    function labelFor() {
        if (selectedIds.length === 0) return placeholder
        if (selectedIds.length === 1) {
            for (var i = 0; i < options.length; i++)
                if (Number(options[i].id) === Number(selectedIds[0])) return options[i].name
        }
        return "已选 " + selectedIds.length + " 个电站"
    }

    // 最近管理推荐:仅未搜索时展示,最多 3 个,且仍需存在于全量列表(被删电站自动消失)
    readonly property var visibleRecent: {
        var out = []
        if (searchText !== "") return out
        for (var i = 0; i < recentOptions.length && out.length < 3; i++) {
            var recent = recentOptions[i]
            for (var j = 0; j < options.length; j++) {
                if (Number(options[j].id) === Number(recent.id) && matchesSearch(options[j].name)) {
                    out.push(options[j]); break
                }
            }
        }
        return out
    }
    // 列表内容:搜索时展示全部匹配项;未搜索且未展开时仅显示前 collapsedCount 个
    readonly property var visibleOptions: {
        var out = []
        for (var i = 0; i < options.length; i++) {
            if (!matchesSearch(options[i].name)) continue
            if (searchText === "" && !expanded && out.length >= collapsedCount) break
            out.push(options[i])
        }
        return out
    }
    readonly property int matchCount: {
        var n = 0
        for (var i = 0; i < options.length; i++) if (matchesSearch(options[i].name)) n++
        return n
    }

    implicitWidth: 200
    implicitHeight: Theme.controlHeight

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.backgroundSecondary
        border.color: control.activeFocus ? Theme.focusRing : Theme.borderSubtle
    }
    contentItem: RowLayout {
        spacing: 6
        Text {
            Layout.fillWidth: true
            leftPadding: 12
            text: control.labelFor()
            color: control.selectedIds.length ? Theme.textPrimary : Theme.textMuted
            font.pixelSize: Theme.fontBody
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            text: optionsPopup.visible ? "▴" : "▾"
            color: Theme.textMuted
            font.pixelSize: Theme.fontCaption
            rightPadding: 10
            verticalAlignment: Text.AlignVCenter
        }
    }

    onClicked: optionsPopup.visible ? optionsPopup.close() : optionsPopup.open()

    Popup {
        id: optionsPopup
        parent: control
        y: control.height + 4
        width: Math.max(control.width, 300)
        height: Math.min(480, optionsColumn.implicitHeight + 20)
        padding: 8
        // 按下"父项以外区域"或 Esc 关闭;点触发器本身由 onClicked 切换,避免关了又开
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
        background: Rectangle { color: Theme.surface; border.color: Theme.borderSubtle; radius: Theme.radiusSmall }
        onOpened: { searchInput.text = ""; control.searchText = ""; control.expanded = false }

        contentItem: ColumnLayout {
            id: optionsColumn
            spacing: 6
            SearchField {
                id: searchInput
                Layout.fillWidth: true
                placeholderText: "搜索电站名称"
                onTextChanged: { control.searchText = text; control.expanded = true }
            }
            Text { visible: control.visibleRecent.length > 0; text: "最近管理"; color: Theme.textMuted; font.pixelSize: Theme.fontCaption; leftPadding: 4 }
            Repeater { model: control.visibleRecent; delegate: optionDelegate }
            Text { text: "全部电站"; color: Theme.textMuted; font.pixelSize: Theme.fontCaption; leftPadding: 4 }
            ListView {
                id: optionsList
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 240)
                clip: true
                model: control.visibleOptions
                delegate: optionDelegate
                boundsBehavior: Flickable.StopAtBounds
                ScrollIndicator.vertical: ScrollIndicator { }
            }
            Text { visible: control.matchCount === 0; text: "没有匹配的电站"; color: Theme.textMuted; font.pixelSize: Theme.fontCaption; leftPadding: 4 }
            AppButton {
                visible: control.matchCount > control.collapsedCount && control.searchText === ""
                Layout.fillWidth: true
                text: control.expanded ? "收起" : "展开全部（共 " + control.matchCount + " 个）"
                variant: "secondary"
                onClicked: control.expanded = !control.expanded
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton { text: "清除已选"; variant: "secondary"; enabled: control.selectedIds.length > 0; onClicked: control.selectionChanged([]) }
                Item { Layout.fillWidth: true }
                AppButton { text: "完成"; onClicked: optionsPopup.close() }
            }
        }
    }

    Component {
        id: optionDelegate
        Item {
            required property var modelData
            width: optionsPopup.availableWidth
            height: 34
            property bool checked: {
                var sel = control.selectedIds
                for (var i = 0; i < sel.length; i++) if (Number(sel[i]) === Number(modelData.id)) return true
                return false
            }
            Rectangle {
                anchors.fill: parent; anchors.margins: 1
                radius: Theme.radiusSmall
                color: rowMouse.containsMouse ? Theme.surfaceHover : "transparent"
            }
            RowLayout {
                anchors.fill: parent; anchors.leftMargin: 10; anchors.rightMargin: 10; spacing: 8
                Rectangle {
                    width: 14; height: 14; radius: 3
                    color: checked ? Theme.accent : "transparent"
                    border.color: checked ? Theme.accent : Theme.borderStrong
                }
                Text { Layout.fillWidth: true; text: modelData.name; color: Theme.textPrimary; font.pixelSize: Theme.fontCaption; elide: Text.ElideRight }
                Text { text: modelData.pile_count + " 桩"; color: Theme.textMuted; font.pixelSize: Theme.fontCaption }
            }
            MouseArea {
                id: rowMouse
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: control.toggleId(modelData.id)
            }
        }
    }
}
