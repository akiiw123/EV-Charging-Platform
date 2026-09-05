import QtQuick
import QtQuick.Controls
import Charging.UI

PanelCard {
    id: root
    property var tableModel
    property var columns: []
    property int selectedRow: -1
    property int totalWidth: { var n=0;for(var i=0;i<columns.length;i++)n+=columns[i].width;return n }
    signal rowActivated(int row, var record)
    clip:true
    ScrollView{anchors.fill:parent;anchors.margins:1;contentWidth:Math.max(root.width-2,root.totalWidth);ScrollBar.horizontal.policy:ScrollBar.AsNeeded
        Column{width:Math.max(root.width-2,root.totalWidth)
            Rectangle{width:parent.width;height:42;color:Theme.backgroundSecondary
                Row{anchors.fill:parent;Repeater{model:root.columns;delegate:Item{required property var modelData;width:modelData.width;height:42
                    Text{anchors.fill:parent;anchors.leftMargin:12;anchors.rightMargin:12;text:modelData.title;color:Theme.textMuted;font.pixelSize:Theme.fontCaption;font.weight:Font.DemiBold;verticalAlignment:Text.AlignVCenter;horizontalAlignment:modelData.align==="right"?Text.AlignRight:modelData.align==="center"?Text.AlignHCenter:Text.AlignLeft;elide:Text.ElideRight}}}}
            }
            ListView{id:list;width:parent.width;height:root.height-44;clip:true;model:root.tableModel;boundsBehavior:Flickable.StopAtBounds
                delegate:Rectangle{required property var record;required property int index;width:list.width;height:Theme.rowHeight;color:root.selectedRow===index?Theme.surfaceSelected:mouse.containsMouse?Theme.surfaceHover:(index%2?Qt.rgba(Theme.backgroundSecondary.r,Theme.backgroundSecondary.g,Theme.backgroundSecondary.b,.28):"transparent")
                    Rectangle{anchors.bottom:parent.bottom;width:parent.width;height:1;color:Theme.borderSubtle}
                    Row{anchors.fill:parent;Repeater{model:root.columns;delegate:Item{required property var modelData;width:modelData.width;height:Theme.rowHeight
                        Loader{anchors.fill:parent;sourceComponent:modelData.role==="status"?badgeCell:textCell
                            property var cellRecord:record;property var cellColumn:modelData}
                    }}}
                    Component {
                        id: textCell
                        Text {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            text: {
                                var value = cellRecord[cellColumn.role]
                                if (cellColumn.format)
                                    return cellColumn.format(value, cellRecord)
                                return value === undefined || value === null ? "—" : value
                            }
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontCaption
                            verticalAlignment: Text.AlignVCenter
                            horizontalAlignment: cellColumn.align === "right" ? Text.AlignRight
                                                 : cellColumn.align === "center" ? Text.AlignHCenter : Text.AlignLeft
                            elide: Text.ElideRight
                        }
                    }
                    Component{id:badgeCell;Item{StatusBadge{anchors.centerIn:parent;status:cellRecord.status;label:root.statusLabel(cellRecord.status)}}}
                    MouseArea{id:mouse;anchors.fill:parent;hoverEnabled:true;cursorShape:Qt.PointingHandCursor;onClicked:{root.selectedRow=index;root.rowActivated(index,record)}}
                    Behavior on color{ColorAnimation{duration:Theme.durationFast}}
                }
                EmptyState{anchors.fill:parent;visible:list.count===0;message:adminController.busy?"正在加载数据…":adminController.loadFailed?"加载失败,请检查服务连接后点击刷新重试":"暂无符合条件的数据"}
            }
        }
    }
    function statusLabel(s){return s==="idle"?"空闲":s==="charging"?"充电中":s==="reserved"?"已预约":s==="awaiting_payment"?"待支付":s==="completed"?"已完成":s==="cancelled"?"已取消":s==="fault"?"故障":s==="offline"?"离线":s==="active"?"正常":s==="frozen"?"已冻结":s}
}
