import QtQuick
import QtQuick.Layouts
import Charging.UI
import "../components"
Item{id:root;property var selected:({});Component.onCompleted:adminController.refreshUsers("","")
    function masked(p){p=String(p||"");return p.length===11?p.slice(0,3)+"****"+p.slice(7):p}
    ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:16
        PageHeader{Layout.fillWidth:true;title:"用户管理";subtitle:"查询用户、钱包与消费状态"}
        RowLayout{Layout.fillWidth:true;SearchField{id:q;placeholderText:"手机号模糊搜索"}FilterComboBox{id:state;model:["全部状态","正常","已冻结"]}AppButton{text:"查询";onClicked:adminController.refreshUsers(q.text,state.currentIndex===1?"active":state.currentIndex===2?"frozen":"")}AppButton{text:"重置";variant:"secondary";onClicked:{q.text="";state.currentIndex=0;adminController.refreshUsers("","")}}Item{Layout.fillWidth:true}}
        DataTable{id:table;Layout.fillWidth:true;Layout.fillHeight:true;tableModel:adminController.usersModel;columns:[{title:"用户 ID",role:"id",width:82},{title:"手机号",role:"phone",width:145,format:function(v){return root.masked(v)}},{title:"昵称",role:"nickname",width:180},{title:"钱包余额",role:"wallet_balance",width:115,align:"right",format:function(v){return "¥"+Number(v).toFixed(2)}},{title:"注册时间",role:"created_at",width:175},{title:"当前状态",role:"status",width:100,align:"center"},{title:"最近活动",role:"last_activity",width:175},{title:"订单数",role:"order_count",width:90,align:"right"},{title:"累计消费",role:"total_spent",width:110,align:"right",format:function(v){return "¥"+Number(v).toFixed(2)}}];onRowActivated:function(row,record){root.selected=record;drawer.open()}}
    }
    DetailDrawer{id:drawer;ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:15
        RowLayout{Rectangle{width:54;height:54;radius:27;color:Theme.surfaceSelected;Text{anchors.centerIn:parent;text:String(root.selected.nickname||"用").charAt(0);color:Theme.accent;font.pixelSize:22;font.bold:true}}ColumnLayout{Text{text:root.selected.nickname||"未设置昵称";color:Theme.textPrimary;font.pixelSize:Theme.fontTitle;font.bold:true}Text{text:root.masked(root.selected.phone);color:Theme.textMuted}}}
        StatusBadge{status:root.selected.status||"active";label:table.statusLabel(status)}
        PanelCard{Layout.fillWidth:true;Layout.preferredHeight:110;ColumnLayout{anchors.fill:parent;anchors.margins:16;Text{text:"钱包余额";color:Theme.textMuted}Text{text:"¥ "+Number(root.selected.wallet_balance||0).toFixed(2);color:Theme.textPrimary;font.pixelSize:28;font.bold:true}}}
        Text{text:"历史订单  "+String(root.selected.order_count||0)+" 单";color:Theme.textSecondary}Text{text:"累计消费  ¥"+Number(root.selected.total_spent||0).toFixed(2);color:Theme.textSecondary}Text{text:"注册时间  "+String(root.selected.created_at||"—");color:Theme.textSecondary}Item{Layout.fillHeight:true}
        AppButton{Layout.fillWidth:true;text:root.selected.status==="active"?"冻结用户":"解除冻结";variant:root.selected.status==="active"?"danger":"secondary";onClicked:statusConfirm.open()}
    }}
    ConfirmDialog{id:statusConfirm;heading:root.selected.status==="active"?"冻结用户":"解除冻结";message:(root.selected.status==="active"?"冻结后该用户将无法登录,已在线的会话也会被强制下线,且不能发起新的预约。":"确认恢复该用户的登录与充电权限。")+"\n用户："+root.masked(root.selected.phone);confirmText:root.selected.status==="active"?"确认冻结":"确认解冻";dangerous:root.selected.status==="active";onAcceptedAction:{drawer.close();adminController.setUserStatus(Number(root.selected.id),root.selected.status==="active"?"frozen":"active")}}
}

