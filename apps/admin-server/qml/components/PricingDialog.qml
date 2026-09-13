import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI

// 站点计价规则编辑:分时电价段(当日分钟区间,左闭右开) + 占位费参数
// (免费挪车时间/每分钟费率/封顶,占位费不受启用开关影响)。
// 打开时拉取当前规则;保存走 admin.pricing.set,时段校验由服务端完成,
// 越界/重叠/非法类型时整体失败并以提示条反馈。保存后立即对后续充电计费生效。
Dialog {
    id: dialog
    property int stationId: 0
    property string stationName: ""
    property var periods: []   // 编辑态行 [{start,end,type,price}](均为字符串,保存时统一校验)

    width: 760
    modal: true
    anchors.centerIn: Overlay.overlay
    padding: 24
    background: PanelCard {}

    onAboutToShow: { validation.text = ""; adminController.loadPricing(dialog.stationId) }

    Connections {
        target: adminController
        function onPricingChanged() { dialog.fill(adminController.pricingDetail) }
    }

    function fill(detail) {
        // 电站不匹配的迟到响应直接忽略
        if (Number(detail.station_id) !== Number(dialog.stationId)) return
        var rule = detail.rule
        enabledCombo.currentIndex = (rule && rule.enabled) ? 1 : 0
        freeMove.text = rule ? String(rule.free_move_minutes) : "0"
        feePerMin.text = rule ? String(rule.occupancy_fee_per_minute) : "0"
        feeCap.text = rule ? String(rule.occupancy_fee_cap) : "0"
        var rows = []
        var src = detail.periods || []
        for (var i = 0; i < src.length; i++) {
            rows.push({start: String(src[i].start_minute), end: String(src[i].end_minute),
                       type: src[i].period_type, price: String(src[i].price_per_kwh)})
        }
        dialog.periods = rows
    }

    function save() {
        validation.text = ""
        var fm = parseInt(freeMove.text)
        var fpm = parseFloat(feePerMin.text)
        var cap = parseFloat(feeCap.text)
        if (isNaN(fm) || fm < 0 || isNaN(fpm) || fpm < 0 || isNaN(cap) || cap < 0) {
            validation.text = "占位费参数需为非负数字"
            return
        }
        var arr = []
        for (var i = 0; i < dialog.periods.length; i++) {
            var row = dialog.periods[i]
            var s = parseInt(row.start), e = parseInt(row.end), p = parseFloat(row.price)
            if (isNaN(s) || isNaN(e) || isNaN(p) || s < 0 || e > 1440 || s >= e || p < 0) {
                validation.text = "第 " + (i + 1) + " 行时段无效:分钟取值 0~1440、开始早于结束、单价非负"
                return
            }
            arr.push({start_minute: s, end_minute: e, period_type: row.type, price_per_kwh: p})
        }
        adminController.savePricing({station_id: dialog.stationId,
                                     enabled: enabledCombo.currentIndex === 1,
                                     free_move_minutes: fm, occupancy_fee_per_minute: fpm,
                                     occupancy_fee_cap: cap, periods: arr})
        dialog.close()
    }

    contentItem: ColumnLayout {
        spacing: 12
        Text { text: "计价规则 · " + dialog.stationName; color: Theme.textPrimary; font.pixelSize: Theme.fontTitle; font.bold: true }
        Text {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: "未启用分时电价或时段未覆盖的时刻,按电站固定单价计费。时段区间为当日分钟数(左闭右开),如 480 表示 08:00,跨零点时段请拆成两条。"
            color: Theme.textMuted; font.pixelSize: Theme.fontCaption
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 10
            ColumnLayout {
                spacing: 4
                Text { text: "计费方式"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                FilterComboBox { id: enabledCombo; Layout.preferredWidth: 130; model: ["仅固定电价", "启用分时电价"] }
            }
            ColumnLayout {
                spacing: 4
                Text { text: "免费挪车(分钟)"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                AppTextField { id: freeMove; Layout.preferredWidth: 110; placeholderText: "如 15" }
            }
            ColumnLayout {
                spacing: 4
                Text { text: "占位费(元/分钟)"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                AppTextField { id: feePerMin; Layout.preferredWidth: 110; placeholderText: "如 0.50" }
            }
            ColumnLayout {
                spacing: 4
                Text { text: "占位费封顶(元)"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                AppTextField { id: feeCap; Layout.preferredWidth: 110; placeholderText: "0 表示不封顶" }
            }
            Item { Layout.fillWidth: true }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Text { Layout.preferredWidth: 110; text: "开始(分钟)"; color: Theme.textMuted; font.pixelSize: Theme.fontCaption; font.weight: Font.DemiBold }
            Text { Layout.preferredWidth: 110; text: "结束(分钟)"; color: Theme.textMuted; font.pixelSize: Theme.fontCaption; font.weight: Font.DemiBold }
            Text { Layout.preferredWidth: 120; text: "类型"; color: Theme.textMuted; font.pixelSize: Theme.fontCaption; font.weight: Font.DemiBold }
            Text { Layout.preferredWidth: 120; text: "单价(元/度)"; color: Theme.textMuted; font.pixelSize: Theme.fontCaption; font.weight: Font.DemiBold }
            Item { Layout.fillWidth: true }
        }
        Repeater {
            model: dialog.periods
            delegate: RowLayout {
                required property var modelData
                required property int index
                AppTextField { Layout.preferredWidth: 110; text: modelData.start; onTextChanged: dialog.periods[index].start = text }
                AppTextField { Layout.preferredWidth: 110; text: modelData.end; onTextChanged: dialog.periods[index].end = text }
                FilterComboBox {
                    Layout.preferredWidth: 120
                    model: ["峰", "平", "谷"]
                    currentIndex: modelData.type === "peak" ? 0 : modelData.type === "flat" ? 1 : 2
                    onActivated: dialog.periods[index].type = ["peak", "flat", "valley"][currentIndex]
                }
                AppTextField { Layout.preferredWidth: 120; text: modelData.price; onTextChanged: dialog.periods[index].price = text }
                AppButton {
                    text: "删除"; variant: "danger"; implicitHeight: 34
                    onClicked: {
                        var rows = dialog.periods.slice()
                        rows.splice(index, 1)
                        dialog.periods = rows
                    }
                }
            }
        }
        AppButton {
            text: "添加时段"; variant: "secondary"
            onClicked: dialog.periods = dialog.periods.concat(
                [{start: "0", end: "480", type: "valley", price: "0.80"}])
        }
        Text { id: validation; color: Theme.danger; font.pixelSize: Theme.fontCaption; visible: text.length > 0; Layout.fillWidth: true; wrapMode: Text.WordWrap }
        RowLayout {
            Layout.alignment: Qt.AlignRight
            AppButton { text: "取消"; variant: "secondary"; onClicked: dialog.close() }
            AppButton {
                text: "保存"; enabled: !adminController.busy
                onClicked: dialog.save()
            }
        }
    }
}
