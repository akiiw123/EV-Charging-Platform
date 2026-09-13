import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI
import "../components"

// 工作台:紧凑指标网格 + 营收趋势线图 + 电桩状态统计 + 各站充电量表
AppScrollView{id:root;contentWidth:availableWidth;clip:true
    Component.onCompleted:adminController.refreshDashboard()
    ColumnLayout{x:24;width:root.availableWidth-48;spacing:16
        PageHeader{Layout.fillWidth:true;title:"工作台";subtitle:"营收、订单与设备状态汇总"}

        // 运营指标:单卡内的 3x3 紧凑网格,不做独立大数字卡
        PanelCard{Layout.fillWidth:true;implicitHeight:232
            GridLayout{anchors.fill:parent;anchors.margins:8;columns:3;rowSpacing:0;columnSpacing:0
                Repeater{model:[
                    {label:"今日营收",value:"¥ "+Number(adminController.dashboard.today_revenue||0).toFixed(2)},
                    {label:"本月营收",value:"¥ "+Number(adminController.dashboard.month_revenue||0).toFixed(2)},
                    {label:"累计营收",value:"¥ "+Number(adminController.dashboard.total_revenue||0).toFixed(2)},
                    {label:"已完成订单(今日)",value:String(adminController.dashboard.completed_orders_today||0)+" 单"},
                    {label:"平均订单金额",value:"¥ "+Number(adminController.dashboard.avg_order_amount||0).toFixed(2)},
                    {label:"注册用户",value:String(adminController.dashboard.registered_users||0)+" 人"},
                    {label:"在线电桩",value:String(adminController.dashboard.online_piles||0)+" 台"},
                    {label:"故障电桩",value:String(adminController.dashboard.fault_piles||0)+" 台",alert:Number(adminController.dashboard.fault_piles||0)>0},
                    {label:"平均在线率",value:Number(adminController.dashboard.online_rate||0).toFixed(1)+" %"}
                ]
                delegate:Item{required property var modelData;required property int index;Layout.fillWidth:true;Layout.preferredHeight:72
                    Rectangle{visible:index%3!==0;anchors.left:parent.left;anchors.top:parent.top;anchors.bottom:parent.bottom;width:1;color:Theme.borderSubtle}
                    Rectangle{visible:index>=3;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;height:1;color:Theme.borderSubtle}
                    ColumnLayout{anchors.verticalCenter:parent.verticalCenter;anchors.left:parent.left;anchors.leftMargin:20;spacing:4
                        Text{text:modelData.label;color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}
                        Text{text:modelData.value;color:modelData.alert?Theme.danger:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.weight:Font.Bold}
                    }
                }
            }
        }
        }

        RowLayout{Layout.fillWidth:true;spacing:16
            // 营收趋势:简单细线图,无渐变无装饰
            PanelCard{Layout.fillWidth:true;Layout.preferredHeight:280
                ColumnLayout{anchors.fill:parent;anchors.margins:18;spacing:8
                    RowLayout{Layout.fillWidth:true;spacing:8
                        Text{text:"近 "+Number(adminController.dashboard.trend_days||30)+" 日营收趋势";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true;Layout.fillWidth:true}
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
                    Canvas{id:lineChart;Layout.fillWidth:true;Layout.fillHeight:true;property var values:adminController.revenueTrend;onValuesChanged:requestPaint();onPaint:{var c=getContext("2d");c.reset();var w=width,h=height;c.strokeStyle=Theme.borderSubtle;c.lineWidth=1;for(var g=1;g<5;g++){c.beginPath();c.moveTo(0,g*h/5);c.lineTo(w,g*h/5);c.stroke()}if(!values||values.length<1)return;var max=1;for(var i=0;i<values.length;i++)max=Math.max(max,Number(values[i].amount));c.strokeStyle=Theme.accent;c.lineWidth=2;c.beginPath();for(i=0;i<values.length;i++){var x=values.length===1?w/2:i*w/(values.length-1),y=h-18-Number(values[i].amount)*(h-36)/max;if(i===0)c.moveTo(x,y);else c.lineTo(x,y)}c.stroke();c.fillStyle=Theme.textMuted;c.font="10px sans-serif";c.textAlign="center";var labelCount=Math.min(5,values.length);for(var li=0;li<labelCount;li++){var idx=labelCount===1?0:Math.round(li*(values.length-1)/(labelCount-1));var lx=values.length===1?w/2:idx*w/(values.length-1);c.fillText(String(values[idx].date||"").slice(5),Math.min(w-18,Math.max(18,lx)),h-4)}
                }
                    Text{visible:!lineChart.values||lineChart.values.length===0;anchors.centerIn:parent;text:"暂无营收数据,完成订单后将在此展示趋势";color:Theme.textMuted;font.pixelSize:Theme.fontBody}
                    }
                }}
            // 电桩状态统计:表格化,含占比细条
            PanelCard{Layout.preferredWidth:360;Layout.fillHeight:true
                ColumnLayout{anchors.fill:parent;anchors.margins:18;spacing:10
                    Text{text:"电桩状态";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}
                    Repeater{model:[["空闲","idle",Theme.success],["充电中","charging",Theme.accent],["故障","fault",Theme.danger],["离线","offline",Theme.textMuted]]
                        delegate:ColumnLayout{required property var modelData;Layout.fillWidth:true;spacing:5
                            RowLayout{Layout.fillWidth:true;spacing:8
                                Text{text:modelData[0];color:Theme.textSecondary;font.pixelSize:Theme.fontBody;Layout.preferredWidth:56}
                                Text{text:String(adminController.pileStatus[modelData[1]]||0)+" 台";color:Theme.textPrimary;font.pixelSize:Theme.fontBody;font.weight:Font.Bold}
                                Item{Layout.fillWidth:true}
                                Text{color:Theme.textMuted;font.pixelSize:Theme.fontCaption;text:{var total=0,keys=["idle","charging","fault","offline"];for(var i=0;i<4;i++)total+=Number(adminController.pileStatus[keys[i]]||0);var n=Number(adminController.pileStatus[modelData[1]]||0);return (total?Math.round(n*100/total):0)+"%"}}
                            }
                            Rectangle{Layout.fillWidth:true;height:4;radius:2;color:Theme.backgroundSecondary
                                Rectangle{width:parent.width*(Number(adminController.pileStatus[modelData[1]]||0)/Math.max(1,totalPileCount()));height:parent.height;radius:2;color:modelData[2]}
                            }
                        }
                    }
                    Item{Layout.fillHeight:true}
                }
            }
        }
        // 各站累计充电量:表格行
        PanelCard{Layout.fillWidth:true;Layout.preferredHeight:64+adminController.stationEnergy.length*32
            ColumnLayout{anchors.fill:parent;anchors.margins:18;spacing:10
                Text{text:"各站累计充电量";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}
                Repeater{model:adminController.stationEnergy;delegate:RowLayout{required property var modelData;Layout.fillWidth:true;Layout.preferredHeight:24;spacing:12
                    Text{Layout.preferredWidth:180;text:modelData.name;color:Theme.textSecondary;font.pixelSize:Theme.fontBody;elide:Text.ElideRight}
                    Rectangle{Layout.fillWidth:true;height:6;radius:3;color:Theme.backgroundSecondary
                        Rectangle{width:parent.width*(Number(modelData.energy||0)/Math.max(1,maxStationEnergy()));height:parent.height;radius:3;color:Theme.accent}
                    }
                    Text{Layout.preferredWidth:90;horizontalAlignment:Text.AlignRight;text:Number(modelData.energy||0).toFixed(1)+" kWh";color:Theme.textPrimary;font.pixelSize:Theme.fontBody}
                }}
                Text{visible:adminController.stationEnergy.length===0;text:"暂无充电量数据";color:Theme.textMuted;font.pixelSize:Theme.fontBody}
            }
        }
    }
    function totalPileCount(){var total=0,keys=["idle","charging","fault","offline"];for(var i=0;i<4;i++)total+=Number(adminController.pileStatus[keys[i]]||0);return total}
    function maxStationEnergy(){var max=1;for(var i=0;i<adminController.stationEnergy.length;i++)max=Math.max(max,Number(adminController.stationEnergy[i].energy||0));return max}
}
