import QtQuick
import QtQuick.Layouts
import Charging.UI
import "../components"
Item{id:root;property var selected:({});Component.onCompleted:adminController.refreshOrders("","")
    function duration(v){var n=Number(v||0),h=Math.floor(n/3600),m=Math.floor((n%3600)/60);return (h?h+"小时 ":"")+m+"分钟"}
    ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:16
        PageHeader{Layout.fillWidth:true;title:"订单管理";subtitle:"追踪预约、充电、结算的完整生命周期"}
        RowLayout{Layout.fillWidth:true;SearchField{id:q;implicitWidth:300;placeholderText:"订单号 / 手机号 / 电站 / 电桩"}FilterComboBox{id:state;model:["全部状态","已预约","充电中","待支付","已完成","已取消"]}AppButton{text:"查询";onClicked:adminController.refreshOrders(q.text,["","reserved","charging","awaiting_payment","completed","cancelled"][state.currentIndex])}AppButton{text:"重置";variant:"secondary";onClicked:{q.text="";state.currentIndex=0;adminController.refreshOrders("","")}}Item{Layout.fillWidth:true}}
        DataTable{id:table;Layout.fillWidth:true;Layout.fillHeight:true;tableModel:adminController.ordersModel;columns:[{title:"订单编号",role:"order_no",width:105},{title:"用户",role:"phone",width:135},{title:"电站",role:"station_name",width:190},{title:"电桩",role:"pile_code",width:120},{title:"状态",role:"status",width:100,align:"center"},{title:"创建时间",role:"created_at",width:165},{title:"开始时间",role:"started_at",width:165},{title:"结束时间",role:"ended_at",width:165},{title:"时长",role:"duration_seconds",width:100,align:"right",format:function(v){return root.duration(v)}},{title:"电量",role:"energy_kwh",width:90,align:"right",format:function(v){return Number(v).toFixed(2)+" kWh"}},{title:"金额",role:"amount",width:90,align:"right",format:function(v){return "¥"+Number(v).toFixed(2)}}];onRowActivated:function(row,record){root.selected=record;drawer.open()}}
    }
    DetailDrawer{id:drawer;ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:16
        Text{text:"订单详情";color:Theme.textPrimary;font.pixelSize:Theme.fontTitle;font.bold:true}Text{text:root.selected.order_no||"—";color:Theme.accent;font.pixelSize:24;font.bold:true}StatusBadge{status:root.selected.status||"cancelled";label:table.statusLabel(status)}
        Repeater{model:[{t:"创建订单",v:root.selected.created_at},{t:"开始充电",v:root.selected.started_at},{t:"结束充电",v:root.selected.ended_at},{t:"完成结算",v:root.selected.status==="completed"?root.selected.ended_at:"尚未完成"}];delegate:RowLayout{required property var modelData;Layout.fillWidth:true;Rectangle{width:10;height:10;radius:5;color:modelData.v?Theme.accent:Theme.textMuted}ColumnLayout{Layout.fillWidth:true;Text{text:modelData.t;color:Theme.textPrimary;font.pixelSize:Theme.fontBody;font.bold:true}Text{text:modelData.v||"尚未发生";color:Theme.textMuted;font.pixelSize:Theme.fontCaption}}}}
        Rectangle{Layout.fillWidth:true;height:1;color:Theme.borderSubtle}Text{text:"充电量  "+Number(root.selected.energy_kwh||0).toFixed(3)+" kWh";color:Theme.textSecondary}Text{text:"订单金额  ¥"+Number(root.selected.amount||0).toFixed(2);color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}Item{Layout.fillHeight:true}
    }}
}

