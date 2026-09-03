import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI
import "../components"
Item{id:root;property var selected:({});Component.onCompleted:adminController.refreshPiles("","","","")
    ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:16
        PageHeader{Layout.fillWidth:true;title:"电桩管理";subtitle:"查看电桩状态与运行记录"}
        RowLayout{Layout.fillWidth:true;SearchField{id:q;placeholderText:"搜索电桩编号"}FilterComboBox{id:type;model:["全部类型","快充","慢充"]}FilterComboBox{id:state;model:["全部状态","空闲","充电中","故障","离线"]}AppButton{text:"查询";onClicked:adminController.refreshPiles(q.text,"",type.currentIndex===1?"fast":type.currentIndex===2?"slow":"",["","idle","charging","fault","offline"][state.currentIndex])}Item{Layout.fillWidth:true}AppButton{text:"刷新";variant:"secondary";onClicked:adminController.refreshPiles(q.text,"","","")}AppButton{text:"新增电桩";onClicked:{root.selected={};editor.open()}}}
        DataTable{id:table;Layout.fillWidth:true;Layout.fillHeight:true;tableModel:adminController.pilesModel;columns:[{title:"电桩编号",role:"code",width:150},{title:"所属电站",role:"station_name",width:230},{title:"类型",role:"type",width:90,format:function(v){return v==="fast"?"快充":"慢充"}},{title:"额定功率",role:"power_kw",width:110,align:"right",format:function(v){return Number(v).toFixed(1)+" kW"}},{title:"当前状态",role:"status",width:110,align:"center"},{title:"累计次数",role:"charge_count",width:100,align:"right"},{title:"累计时长",role:"total_charge_minutes",width:120,align:"right",format:function(v){return Number(v)+" 分钟"}},{title:"最近心跳",role:"last_heartbeat",width:180,format:function(){return "暂无心跳字段"}}];onRowActivated:function(row,record){root.selected=record;drawer.open()}}
    }
    DetailDrawer{id:drawer;ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:14;Text{text:"设备详情";color:Theme.textPrimary;font.pixelSize:Theme.fontTitle;font.bold:true}StatusBadge{status:root.selected.status||"offline";label:table.statusLabel(status)}Text{text:root.selected.code||"—";color:Theme.textPrimary;font.pixelSize:24;font.bold:true}Text{text:root.selected.station_name||"—";color:Theme.textSecondary;font.pixelSize:Theme.fontBody}Text{text:"额定功率  "+Number(root.selected.power_kw||0).toFixed(1)+" kW";color:Theme.textSecondary}Text{text:"累计充电  "+String(root.selected.charge_count||0)+" 次";color:Theme.textSecondary}Item{Layout.fillHeight:true}AppButton{Layout.fillWidth:true;text:"远程重启";variant:"danger";enabled:root.selected.status!=="charging";onClicked:restartConfirm.open()}Text{visible:root.selected.status==="charging";text:"充电中的设备不能远程重启";color:Theme.warning;font.pixelSize:Theme.fontCaption}
            Rectangle{Layout.fillWidth:true;height:1;color:Theme.borderSubtle}
            Text{text:"手工状态切换";color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}
            RowLayout{Layout.fillWidth:true;spacing:8
                AppButton{Layout.fillWidth:true;text:"恢复空闲";variant:"secondary";enabled:root.selected.status!=="charging"&&root.selected.status!=="idle";onClicked:adminController.setPileStatus(Number(root.selected.id),"idle")}
                AppButton{Layout.fillWidth:true;text:"设为故障";variant:"danger";enabled:root.selected.status!=="charging"&&root.selected.status!=="fault";onClicked:adminController.setPileStatus(Number(root.selected.id),"fault")}
                AppButton{Layout.fillWidth:true;text:"设为离线";variant:"secondary";enabled:root.selected.status!=="charging"&&root.selected.status!=="offline";onClicked:adminController.setPileStatus(Number(root.selected.id),"offline")}
            }
            Text{visible:root.selected.status==="charging";text:"充电中的设备不能手工切换状态";color:Theme.warning;font.pixelSize:Theme.fontCaption}
            AppButton{Layout.fillWidth:true;text:"编辑类型/功率";variant:"secondary";enabled:root.selected.status!=="charging";onClicked:{drawer.close();editor.open()}}}}
    ConfirmDialog{id:restartConfirm;heading:"确认远程重启";message:"将重启电桩 "+(root.selected.code||"")+"（"+(root.selected.station_name||"")+"）。操作期间请勿重复提交。";confirmText:"执行重启";dangerous:true;onAcceptedAction:{drawer.close();adminController.restartPile(Number(root.selected.id))}}

    Dialog{id:editor;modal:true;anchors.centerIn:Overlay.overlay;width:460;padding:24;property bool editing:root.selected.id!==undefined
        onOpened:{if(editing){editCode.text=root.selected.code||"";editType.currentIndex=root.selected.type==="slow"?1:0;editPower.text=String(root.selected.power_kw||"")}else{editCode.text="";editType.currentIndex=0;editPower.text=""}}
        background:PanelCard{}
        contentItem:ColumnLayout{spacing:12
            Text{text:editor.editing?"编辑电桩":"新增电桩";color:Theme.textPrimary;font.pixelSize:Theme.fontTitle;font.bold:true}
            Text{visible:!editor.editing;text:"所属电站 *";color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}
            FilterComboBox{id:editStation;visible:!editor.editing;Layout.fillWidth:true;model:adminController.stationNames()}
            Text{text:"电桩编号 *";color:Theme.textSecondary;font.pixelSize:Theme.fontCaption;visible:!editor.editing}
            AppTextField{id:editCode;Layout.fillWidth:true;visible:!editor.editing;placeholderText:"如 A-01(全局唯一)"}
            Text{text:"类型";color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}
            FilterComboBox{id:editType;Layout.fillWidth:true;model:["快充","慢充"]}
            Text{text:"额定功率 (kW)";color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}
            AppTextField{id:editPower;Layout.fillWidth:true;placeholderText:"0 ~ 1000"}
            Text{id:editValidation;color:Theme.danger;font.pixelSize:Theme.fontCaption;visible:text.length>0}
            RowLayout{Layout.alignment:Qt.AlignRight;AppButton{text:"取消";variant:"secondary";onClicked:editor.close()}AppButton{text:"保存";onClicked:{
                var pw=Number(editPower.text);
                if(!editor.editing&&editStation.currentIndex<0){editValidation.text="请先在『电站管理』刷新电站列表";return}
                if(editCode.text.trim()===""||pw<=0||pw>1000){editValidation.text="请填写编号并检查功率范围 (0,1000]";return}
                if(editor.editing)adminController.updatePile({pile_id:Number(root.selected.id),type:editType.currentIndex===1?"slow":"fast",power_kw:pw});
                else adminController.createPile({station_id:Number(adminController.stationAt(editStation.currentIndex).id),code:editCode.text.trim(),type:editType.currentIndex===1?"slow":"fast",power_kw:pw});
                editor.close()}}}
        }}
}
