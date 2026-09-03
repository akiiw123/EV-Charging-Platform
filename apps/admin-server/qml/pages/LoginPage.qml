import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Charging.UI

Rectangle{id:root;color:Theme.backgroundPrimary
    Rectangle{anchors.fill:parent;gradient:Gradient{orientation:Gradient.Horizontal;GradientStop{position:0;color:Theme.backgroundPrimary}GradientStop{position:1;color:Theme.backgroundSecondary}}}
    RowLayout{anchors.fill:parent;anchors.margins:Math.max(50,parent.width*.08);spacing:80
        ColumnLayout{Layout.fillWidth:true;Layout.maximumWidth:620;spacing:18
            Rectangle{width:58;height:58;radius:18;color:Theme.accent;LineIcon{anchors.centerIn:parent;width:28;height:28;name:"pile";strokeColor:"white"}}
            Text{text:"电动汽车充电桩\n应用管理平台";color:Theme.textPrimary;font.pixelSize:40;font.weight:Font.Bold;lineHeight:1.18}
            Text{Layout.maximumWidth:520;text:"面向电站、设备、订单与用户的一体化运营管理中枢";color:Theme.textSecondary;font.pixelSize:16;wrapMode:Text.Wrap}
            Row{spacing:22;Repeater{model:["实时运营","设备监控","智能预测"];delegate:Row{required property var modelData;spacing:8;Rectangle{width:7;height:7;radius:4;color:Theme.accent;anchors.verticalCenter:parent.verticalCenter}Text{text:modelData;color:Theme.textMuted;font.pixelSize:Theme.fontBody}}}}
        }
        PanelCard{Layout.preferredWidth:430;Layout.preferredHeight:510
            ColumnLayout{anchors.fill:parent;anchors.margins:38;spacing:14
                Text{text:"运营管理端";color:Theme.textPrimary;font.pixelSize:26;font.bold:true}
                Text{text:"使用管理员凭据安全登录";color:Theme.textMuted;font.pixelSize:Theme.fontBody;Layout.bottomMargin:14}
                Text{text:"管理员账号";color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}
                AppTextField{id:user;Layout.fillWidth:true;text:adminController.savedUsername();placeholderText:"请输入账号";KeyNavigation.tab:pass}
                Text{text:"密码";color:Theme.textSecondary;font.pixelSize:Theme.fontCaption}
                AppTextField{id:pass;Layout.fillWidth:true;placeholderText:"请输入密码";echoMode:show.checked?TextInput.Normal:TextInput.Password;error:adminController.errorMessage.length>0;onAccepted:loginButton.clicked()}
                CheckBox{id:show;text:"显示密码";contentItem:Text{text:show.text;color:Theme.textSecondary;font.pixelSize:Theme.fontCaption;leftPadding:show.indicator.width+8;verticalAlignment:Text.AlignVCenter}}
                Text{Layout.fillWidth:true;text:adminController.errorMessage;color:Theme.danger;font.pixelSize:Theme.fontCaption;wrapMode:Text.Wrap;visible:text.length>0}
                CheckBox{id:remember;checked:user.text.length>0;text:"记住账号";contentItem:Text{text:remember.text;color:Theme.textSecondary;font.pixelSize:Theme.fontCaption;leftPadding:remember.indicator.width+8;verticalAlignment:Text.AlignVCenter}}
                AppButton{id:loginButton;Layout.fillWidth:true;text:"登录管理平台";loading:adminController.busy;onClicked:adminController.login(user.text,pass.text,remember.checked)}
                Item{Layout.fillHeight:true}
                RowLayout{Layout.fillWidth:true;Rectangle{width:8;height:8;radius:4;color:adminController.connected?Theme.success:Theme.warning}Text{text:adminController.connected?"服务连接正常":"正在连接服务";color:Theme.textMuted;font.pixelSize:Theme.fontCaption}Item{Layout.fillWidth:true}Text{text:"v0.2.0";color:Theme.textMuted;font.pixelSize:Theme.fontCaption}}
            }
        }
    }
}

