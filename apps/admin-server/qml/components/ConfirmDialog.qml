/*
 * 文件职责：危险操作确认框：显示明确目标并防止删除、重启、冻结等操作误触。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI
Dialog{id:dialog;property string heading:"确认操作";property string message:"";property string confirmText:"确认";property bool dangerous:false;signal acceptedAction();modal:true;anchors.centerIn:Overlay.overlay;width:420;padding:22
    background:PanelCard{}
    contentItem:ColumnLayout{spacing:18
        Text{text:dialog.heading;color:Theme.textPrimary;font.pixelSize:Theme.fontTitle;font.bold:true}
        Text{Layout.fillWidth:true;text:dialog.message;color:Theme.textSecondary;font.pixelSize:Theme.fontBody;wrapMode:Text.Wrap}
        RowLayout{Layout.alignment:Qt.AlignRight;AppButton{text:"取消";variant:"secondary";onClicked:dialog.close()}AppButton{text:dialog.confirmText;variant:dialog.dangerous?"danger":"primary";enabled:!adminController.busy;onClicked:{dialog.close();dialog.acceptedAction()}}}
    }}
