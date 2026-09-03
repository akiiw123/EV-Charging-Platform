import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI
Dialog{id:dialog;property string heading:"确认操作";property string message:"";property string confirmText:"确认";property bool dangerous:false;signal acceptedAction();modal:true;anchors.centerIn:Overlay.overlay;width:420;padding:22
    background:PanelCard{}
    contentItem:ColumnLayout{spacing:18
        Text{text:dialog.heading;color:Theme.textPrimary;font.pixelSize:Theme.fontTitle;font.bold:true}
        Text{Layout.fillWidth:true;text:dialog.message;color:Theme.textSecondary;font.pixelSize:Theme.fontBody;wrapMode:Text.Wrap}
        RowLayout{Layout.alignment:Qt.AlignRight;AppButton{text:"取消";variant:"secondary";onClicked:dialog.close()}AppButton{text:dialog.confirmText;variant:dialog.dangerous?"danger":"primary";onClicked:{dialog.close();dialog.acceptedAction()}}}
    }}
