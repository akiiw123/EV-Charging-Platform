import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI
import "../components"

Item {
    id: root
    property var selected: ({})
    // "去管理电桩"跳转请求:AppShell 接到后切换到电桩管理页并预筛选该站
    signal managePilesRequested(var station)

    // 三级区域筛选状态:空串=该级不限制;"未分区"由控制器匹配区域为空的电站
    property string provinceSel: ""
    property string citySel: ""
    property string districtSel: ""

    Component.onCompleted: {
        adminController.refreshStations("")
        adminController.refreshPiles("", [], "", "")
        rebuildRegionModels(true)
    }

    function applyRegionFilter() {
        adminController.setStationRegionFilter(root.provinceSel, root.citySel, root.districtSel)
    }

    function sameList(a, b) {
        if (a.length !== b.length) return false
        for (var i = 0; i < a.length; i++) if (String(a[i]) !== String(b[i])) return false
        return true
    }

    // 电站数据变化(登录后首载/新增/编辑)后重建级联选项;选项未变化时不重建,
    // 避免每 5 秒自动刷新带来的无谓刷新。尽量保持当前选中项,失效则回退到"全部"。
    function rebuildRegionModels(force) {
        var provinces = ["全部省份"].concat(adminController.stationProvinces())
        var cities = ["全部城市"].concat(adminController.stationCities(root.provinceSel))
        var districts = ["全部区县"].concat(adminController.stationDistricts(root.provinceSel, root.citySel))
        if (!force && sameList(provinceFilter.model, provinces)
                && sameList(cityFilter.model, cities) && sameList(districtFilter.model, districts)) return
        provinceFilter.model = provinces
        var pi = root.provinceSel === "" ? 0 : provinces.indexOf(root.provinceSel)
        if (pi < 0) { root.provinceSel = ""; root.citySel = ""; root.districtSel = ""; pi = 0 }
        provinceFilter.currentIndex = pi
        cityFilter.model = ["全部城市"].concat(adminController.stationCities(root.provinceSel))
        var ci = root.citySel === "" ? 0 : cityFilter.model.indexOf(root.citySel)
        if (ci < 0) { root.citySel = ""; root.districtSel = ""; ci = 0 }
        cityFilter.currentIndex = ci
        districtFilter.model = ["全部区县"].concat(adminController.stationDistricts(root.provinceSel, root.citySel))
        var di = root.districtSel === "" ? 0 : districtFilter.model.indexOf(root.districtSel)
        if (di < 0) { root.districtSel = ""; di = 0 }
        districtFilter.currentIndex = di
        root.applyRegionFilter()
    }

    Connections {
        target: adminController
        function onStationDataChanged() { root.rebuildRegionModels(false) }
    }

    ColumnLayout { anchors.fill: parent; anchors.margins: 24; spacing: 16
        PageHeader { Layout.fillWidth: true; title: "电站管理"; subtitle: "电站资料与价格维护" }
        RowLayout { Layout.fillWidth: true; spacing: 8
            SearchField { id: search; Layout.fillWidth: true; Layout.maximumWidth: 220; placeholderText: "搜索电站名称或地址"; onAccepted: adminController.refreshStations(text) }
            // 省→市→区三级联动:上级变化时重置下级并立即应用(客户端过滤,无需点查询)
            FilterComboBox { id: provinceFilter; Layout.preferredWidth: 126; model: ["全部省份"]
                onActivated: { root.provinceSel = currentIndex === 0 ? "" : currentText; root.citySel = ""; root.districtSel = ""
                    cityFilter.model = ["全部城市"].concat(adminController.stationCities(root.provinceSel)); cityFilter.currentIndex = 0
                    districtFilter.model = ["全部区县"].concat(adminController.stationDistricts(root.provinceSel, "")); districtFilter.currentIndex = 0
                    root.applyRegionFilter() } }
            FilterComboBox { id: cityFilter; Layout.preferredWidth: 126; model: ["全部城市"]
                onActivated: { root.citySel = currentIndex === 0 ? "" : currentText; root.districtSel = ""
                    districtFilter.model = ["全部区县"].concat(adminController.stationDistricts(root.provinceSel, root.citySel)); districtFilter.currentIndex = 0
                    root.applyRegionFilter() } }
            FilterComboBox { id: districtFilter; Layout.preferredWidth: 126; model: ["全部区县"]
                onActivated: { root.districtSel = currentIndex === 0 ? "" : currentText; root.applyRegionFilter() } }
            AppButton { text: "查询"; onClicked: adminController.refreshStations(search.text) }
            AppButton { text: "重置"; variant: "secondary"; onClicked: { search.text = ""; root.provinceSel = ""; root.citySel = ""; root.districtSel = ""; root.rebuildRegionModels(true); adminController.refreshStations("") } }
            Item { Layout.fillWidth: true }
            AppButton { text: "刷新"; variant: "secondary"; onClicked: adminController.refreshStations(search.text) }
            AppButton { text: "新增电站"; onClicked: { root.selected = {}; editor.open() } }
        }
        DataTable { id: table; Layout.fillWidth: true; Layout.fillHeight: true; tableModel: adminController.stationsModel; columns: [{title:"ID",role:"id",width:64},{title:"电站名称",role:"name",width:190},{title:"所属区域",role:"province",width:190,format:function(v,r){var parts=[];if(r.province)parts.push(r.province);if(r.city&&r.city!==r.province)parts.push(r.city);if(r.district)parts.push(r.district);return parts.length?parts.join(" / "):"未分区"}},{title:"详细地址",role:"address",width:300},{title:"纬度",role:"latitude",width:105},{title:"经度",role:"longitude",width:105},{title:"单价",role:"price_per_kwh",width:90,align:"right",format:function(v){return "¥"+Number(v).toFixed(2)}},{title:"电桩",role:"pile_count",width:78,align:"right"},{title:"空闲",role:"idle_pile_count",width:78,align:"right"},{title:"在线率",role:"online_rate",width:90,align:"right",format:function(v,r){return Number(r.pile_count)?((Number(r.pile_count)-Number(r.offline_count||0))*100/Number(r.pile_count)).toFixed(1)+"%":"—"}},{title:"营业状态",role:"status",width:90,align:"center"},{title:"创建时间",role:"created_at",width:170}];onRowActivated:function(row,record){root.selected=record;drawer.open()}}
    }
    DetailDrawer { id: drawer
        ColumnLayout { anchors.fill: parent; anchors.margins: 24; spacing: 14
            Text { text: "电站详情"; color: Theme.textPrimary; font.pixelSize: Theme.fontTitle; font.bold: true }
            Repeater { model: [{k:"名称",v:root.selected.name},{k:"所属区域",v:(function(){var parts=[];if(root.selected.province)parts.push(root.selected.province);if(root.selected.city&&root.selected.city!==root.selected.province)parts.push(root.selected.city);if(root.selected.district)parts.push(root.selected.district);return parts.length?parts.join(" / "):"未分区"})()},{k:"地址",v:root.selected.address},{k:"坐标",v:String(root.selected.latitude)+", "+String(root.selected.longitude)},{k:"充电单价",v:"¥"+Number(root.selected.price_per_kwh||0).toFixed(2)+" / kWh"},{k:"电桩规模",v:String(root.selected.idle_pile_count||0)+" 空闲 / "+String(root.selected.pile_count||0)+" 总数"}]; delegate: ColumnLayout { required property var modelData; Layout.fillWidth: true; Text { text: modelData.k; color: Theme.textMuted; font.pixelSize: Theme.fontCaption } Text { Layout.fillWidth: true; text: modelData.v || "—"; color: Theme.textPrimary; font.pixelSize: Theme.fontBody; wrapMode: Text.Wrap } } }
            Rectangle { Layout.fillWidth: true; height: 1; color: Theme.borderSubtle }
            Text { text: "站内电桩  " + adminController.pilesOfStationId(Number(root.selected.id || 0)).length + " 台"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
            Repeater { model: adminController.pilesOfStationId(Number(root.selected.id || 0))
                delegate: RowLayout { required property var modelData; Layout.fillWidth: true; spacing: 8
                    Text { Layout.fillWidth: true; text: modelData.code; color: Theme.textPrimary; font.pixelSize: Theme.fontBody }
                    Text { text: (modelData.type === "fast" ? "快充 " : "慢充 ") + Number(modelData.power_kw || 0).toFixed(1) + "kW"; color: Theme.textMuted; font.pixelSize: Theme.fontCaption }
                    StatusBadge { status: modelData.status || "offline"; label: table.statusLabel(status) }
                } }
            // 一键跳转:切到电桩管理页并带上该站预筛选(可在该页清除);同时记入"最近管理"
            AppButton { Layout.fillWidth: true; text: "去管理电桩（" + adminController.pilesOfStationId(Number(root.selected.id || 0)).length + " 台）"; onClicked: { var station = root.selected; adminController.noteStationManaged(Number(station.id || 0)); root.managePilesRequested(station); drawer.close() } }
            RowLayout { Layout.fillWidth: true; spacing: 8
                AppButton { Layout.fillWidth: true; text: root.selected.status === "disabled" ? "恢复营业" : "停用电站"; variant: "secondary"
                    onClicked: statusConfirm.open() }
                Text { Layout.fillWidth: true; text: root.selected.status === "disabled" ? "当前已停用:用户端不可见、不可预约" : "停用后用户端将不再展示该电站,且无法预约其电桩;历史订单保留。"
                    color: Theme.textMuted; font.pixelSize: Theme.fontCaption; wrapMode: Text.Wrap }
            }
            Item { Layout.fillHeight: true } AppButton { Layout.fillWidth: true; text: "编辑电站"; variant: "secondary"; onClicked: { drawer.close(); editor.open() } } AppButton { Layout.fillWidth: true; text: "删除电站"; variant: "danger"; onClicked: deleteConfirm.open() }
        } }
    Dialog { id: editor; modal: true; anchors.centerIn: Overlay.overlay; width: 520; padding: 24; property bool editing: root.selected.id !== undefined
        onOpened: { name.text = editing ? root.selected.name : ""; address.text = editing ? root.selected.address : ""; provinceField.text = editing ? (root.selected.province || "") : ""; cityField.text = editing ? (root.selected.city || "") : ""; districtField.text = editing ? (root.selected.district || "") : ""; lat.text = editing ? root.selected.latitude : ""; lng.text = editing ? root.selected.longitude : ""; price.text = editing ? root.selected.price_per_kwh : ""; count.text = editing ? root.selected.pile_count : "6" }
        background: PanelCard {}
        contentItem: ColumnLayout { spacing: 12
            Text { text: editor.editing ? "编辑电站" : "新增电站"; color: Theme.textPrimary; font.pixelSize: Theme.fontTitle; font.bold: true }
            Text { text: "电站名称 *"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption } AppTextField { id: name; Layout.fillWidth: true }
            Text { text: "详细地址 *"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption } AppTextField { id: address; Layout.fillWidth: true }
            // 行政区划:供三级区域筛选使用,可留空(未分区);选项来源为表单自由填写+服务端校验
            RowLayout { Layout.fillWidth: true; spacing: 8
                ColumnLayout { Layout.fillWidth: true; spacing: 4
                    Text { text: "省份"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                    AppTextField { id: provinceField; Layout.fillWidth: true; placeholderText: "如 北京市" }
                }
                ColumnLayout { Layout.fillWidth: true; spacing: 4
                    Text { text: "城市"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                    AppTextField { id: cityField; Layout.fillWidth: true; placeholderText: "如 北京市" }
                }
                ColumnLayout { Layout.fillWidth: true; spacing: 4
                    Text { text: "区县"; color: Theme.textSecondary; font.pixelSize: Theme.fontCaption }
                    AppTextField { id: districtField; Layout.fillWidth: true; placeholderText: "如 房山区" }
                }
            }
            RowLayout { Layout.fillWidth: true; AppTextField { id: lat; Layout.fillWidth: true; placeholderText: "纬度 -90~90" } AppTextField { id: lng; Layout.fillWidth: true; placeholderText: "经度 -180~180" } }
            RowLayout { Layout.fillWidth: true; AppTextField { id: price; Layout.fillWidth: true; placeholderText: "单价 元/kWh" } AppTextField { id: count; Layout.fillWidth: true; enabled: !editor.editing; placeholderText: "初始电桩 1~100" } }
            Text { id: validation; color: Theme.danger; font.pixelSize: Theme.fontCaption; visible: text.length > 0 }
            RowLayout { Layout.alignment: Qt.AlignRight; AppButton { text: "取消"; variant: "secondary"; onClicked: editor.close() } AppButton { text: "保存"; onClicked: { var la = Number(lat.text), lo = Number(lng.text), pr = Number(price.text), co = Number(count.text); if (!name.text.trim() || !address.text.trim() || la < -90 || la > 90 || lo < -180 || lo > 180 || pr < 0 || (!editor.editing && (co < 1 || co > 100))) { validation.text = "请完整填写表单，并检查坐标、价格和电桩数量"; return } if (provinceField.text.trim().length > 32 || cityField.text.trim().length > 32 || districtField.text.trim().length > 32) { validation.text = "省/市/区长度不能超过 32 个字符"; return } var f = {id: root.selected.id, name: name.text.trim(), address: address.text.trim(), province: provinceField.text.trim(), city: cityField.text.trim(), district: districtField.text.trim(), latitude: la, longitude: lo, price_per_kwh: pr, pile_count: co}; if (editor.editing) adminController.updateStation(f); else adminController.createStation(f); editor.close() } } }
        } }
    ConfirmDialog { id: deleteConfirm; heading: "删除电站"; message: "确认删除“" + (root.selected.name || "") + "”？包含活动订单的电站将被服务端拒绝删除。"; confirmText: "确认删除"; dangerous: true; onAcceptedAction: { drawer.close(); adminController.deleteStation(Number(root.selected.id)) } }
    ConfirmDialog { id: statusConfirm
        heading: root.selected.status === "disabled" ? "恢复营业" : "停用电站"
        message: root.selected.status === "disabled"
            ?"确认恢复“"+(root.selected.name||"")+"”的营业状态?恢复后用户端将重新展示该电站。"
            :"确认停用“"+(root.selected.name||"")+"”?停用后用户端不再展示该电站、其电桩不可预约;历史订单保留,可随时恢复营业。"
        confirmText: root.selected.status === "disabled" ? "确认恢复" : "确认停用"
        // 停用/恢复不携带区域键:服务端按键存在才更新,电站区域不受影响
        onAcceptedAction: { drawer.close(); adminController.updateStation({id:Number(root.selected.id),name:root.selected.name||"",address:root.selected.address||"",latitude:Number(root.selected.latitude||0),longitude:Number(root.selected.longitude||0),price_per_kwh:Number(root.selected.price_per_kwh||0),status:root.selected.status==="disabled"?"active":"disabled"}) } }
}
