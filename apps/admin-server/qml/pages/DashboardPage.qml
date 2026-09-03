import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI
import "../components"

ScrollView{id:root;contentWidth:availableWidth;clip:true
    Component.onCompleted:adminController.refreshDashboard()
    ColumnLayout{x:24;width:root.availableWidth-48;spacing:16
        PageHeader{Layout.fillWidth:true;title:"运营数据总览";subtitle:"实时掌握营收、订单与设备健康状态"}
        GridLayout{Layout.fillWidth:true;columns:4;rowSpacing:12;columnSpacing:12
            MetricCard{Layout.fillWidth:true;label:"今日营收";value:"¥ "+Number(adminController.dashboard.today_revenue||0).toFixed(2);trend:"今日实时"}
            MetricCard{Layout.fillWidth:true;label:"本月营收";value:"¥ "+Number(adminController.dashboard.month_revenue||0).toFixed(2);trend:"本月累计";tone:Theme.info}
            MetricCard{Layout.fillWidth:true;label:"累计营收";value:"¥ "+Number(adminController.dashboard.total_revenue||0).toFixed(2);trend:"历史累计";tone:"#9B87F5"}
            MetricCard{Layout.fillWidth:true;label:"今日订单";value:String(adminController.dashboard.today_orders||0);unit:"单";trend:"实时订单";tone:Theme.warning}
            MetricCard{Layout.fillWidth:true;label:"注册用户";value:String(adminController.dashboard.registered_users||0);unit:"人";trend:"平台用户";tone:Theme.info}
            MetricCard{Layout.fillWidth:true;label:"在线电桩";value:String(adminController.dashboard.online_piles||0);unit:"台";trend:"设备在线";tone:Theme.success}
            MetricCard{Layout.fillWidth:true;label:"故障电桩";value:String(adminController.dashboard.fault_piles||0);unit:"台";trend:Number(adminController.dashboard.fault_piles||0)>0?"需要处理":"运行正常";tone:Theme.danger}
            MetricCard{Layout.fillWidth:true;label:"平均在线率";value:Number(adminController.dashboard.online_rate||0).toFixed(1);unit:"%";trend:"设备可用性";tone:Theme.accent}
        }
        RowLayout{Layout.fillWidth:true;spacing:16
            PanelCard{Layout.fillWidth:true;Layout.preferredHeight:300
                ColumnLayout{anchors.fill:parent;anchors.margins:18
                    RowLayout{Layout.fillWidth:true;spacing:8
                        Text{text:"近 "+Number(adminController.dashboard.trend_days||30)+" 日营收趋势";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true;Layout.fillWidth:true}
                        // 7/30 日统计区间切换:切换后重新请求 admin.dashboard
                        Repeater{model:[7,30];delegate:Button{required property int modelData
                            text:modelData+"日";font.pixelSize:11
                            implicitWidth:44;implicitHeight:24
                            checkable:true
                            checked:Number(adminController.dashboard.trend_days||30)===modelData
                            onClicked:adminController.refreshDashboard(modelData)
                            background:Rectangle{radius:6;color:parent.checked?Theme.accent:"transparent";border.width:1;border.color:parent.checked?Theme.accent:Theme.borderSubtle}
                            contentItem:Text{text:parent.text;font:parent.font;color:parent.checked?"white":Theme.textMuted;horizontalAlignment:Text.AlignHCenter;verticalAlignment:Text.AlignVCenter}
                        }}
                    }
                    Canvas{id:lineChart;Layout.fillWidth:true;Layout.fillHeight:true;property var values:adminController.revenueTrend;onValuesChanged:requestPaint();onPaint:{var c=getContext("2d");c.reset();var w=width,h=height;c.strokeStyle=Theme.borderSubtle;c.lineWidth=1;for(var g=1;g<5;g++){c.beginPath();c.moveTo(0,g*h/5);c.lineTo(w,g*h/5);c.stroke()}if(!values||values.length<1)return;var max=1;for(var i=0;i<values.length;i++)max=Math.max(max,Number(values[i].amount));c.strokeStyle=Theme.accent;c.lineWidth=2;c.beginPath();for(i=0;i<values.length;i++){var x=values.length===1?w/2:i*w/(values.length-1),y=h-18-Number(values[i].amount)*(h-36)/max;if(i===0)c.moveTo(x,y);else c.lineTo(x,y)}c.stroke()}}
                }}
            PanelCard{Layout.preferredWidth:340;Layout.preferredHeight:300
                ColumnLayout{anchors.fill:parent;anchors.margins:18;Text{text:"电桩状态分布";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}
                    Item{Layout.fillWidth:true;Layout.fillHeight:true
                        Canvas{id:donut;anchors.centerIn:parent;width:170;height:170;property var values:adminController.pileStatus;onValuesChanged:requestPaint();onPaint:{var c=getContext("2d");c.reset();var keys=["idle","charging","fault","offline"],colors=[Theme.success,Theme.accent,Theme.danger,Theme.textMuted],total=0,i;for(i=0;i<4;i++)total+=Number(values[keys[i]]||0);var a=-Math.PI/2;for(i=0;i<4;i++){var n=Number(values[keys[i]]||0),end=a+(total?n/total:0)*Math.PI*2;c.beginPath();c.strokeStyle=colors[i];c.lineWidth=18;c.arc(85,85,58,a,end);c.stroke();a=end}}}
                        Text{anchors.centerIn:parent;text:Number(adminController.dashboard.online_rate||0).toFixed(0)+"%\n在线";horizontalAlignment:Text.AlignHCenter;color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}
                    }
                    // 图例同时展示各状态数量占比百分比
                    RowLayout{Layout.fillWidth:true;Repeater{model:[["空闲","idle"],["充电中","charging"],["故障","fault"],["离线","offline"]];delegate:Text{required property var modelData;Layout.fillWidth:true
                        text:{var total=0,keys=["idle","charging","fault","offline"];for(var i=0;i<4;i++)total+=Number(adminController.pileStatus[keys[i]]||0);var n=Number(adminController.pileStatus[modelData[1]]||0);return modelData[0]+" "+(total?Math.round(n*100/total):0)+"%"}
                        color:Theme.textMuted;font.pixelSize:10;horizontalAlignment:Text.AlignHCenter}}}
                }}
        }
        PanelCard{Layout.fillWidth:true;Layout.preferredHeight:220
            ColumnLayout{anchors.fill:parent;anchors.margins:18;Text{text:"各站累计充电量";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}
                RowLayout{Layout.fillWidth:true;Layout.fillHeight:true;spacing:14;Repeater{model:adminController.stationEnergy;delegate:ColumnLayout{required property var modelData;Layout.fillWidth:true;Layout.fillHeight:true
                    Item{Layout.fillHeight:true;Layout.fillWidth:true;Rectangle{anchors.bottom:parent.bottom;anchors.horizontalCenter:parent.horizontalCenter;width:Math.min(46,parent.width*.65);height:Math.max(3,Math.min(parent.height,Number(modelData.energy||0)*2));radius:4;color:Theme.accent}}
                    Text{Layout.fillWidth:true;text:modelData.name;color:Theme.textMuted;font.pixelSize:10;elide:Text.ElideRight;horizontalAlignment:Text.AlignHCenter}}}}
            }}
    }
}
