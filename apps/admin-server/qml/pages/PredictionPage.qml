/*
 * 文件职责：智能预测页：展示独立 ML HTTP 服务返回的站点预测、模型来源及错误/演示状态。
 * 对接关系：属于 AdminAppController；QML 负责展示和交互，业务数据与持久化由 C++/服务端负责。
 * 阅读提示：property 是页面状态，signal 是向上层发出的事件，function 是本页面的轻量交互辅助。
 */
import QtQuick
import QtQuick.Layouts
import Charging.UI
import "../components"
Item{id:root;Component.onCompleted:adminController.refreshPredictions()
    ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:16
        RowLayout{Layout.fillWidth:true;PageHeader{Layout.fillWidth:true;title:"智能预测";subtitle:"未来 1/6/24 小时累计充电量预测"}StatusBadge{status:(adminController.predictionSource==="模型服务"||adminController.predictionSource==="历史回放模型")?"active":"warning";label:adminController.predictionSource}AppButton{text:adminController.predictionLoading?"预测中…":"刷新预测";enabled:!adminController.predictionLoading;variant:"secondary";onClicked:adminController.refreshPredictions()}}
        PanelCard{Layout.fillWidth:true;Layout.preferredHeight:70;RowLayout{anchors.fill:parent;anchors.margins:16;Rectangle{width:10;height:10;radius:5;color:(adminController.predictionSource==="模型服务"||adminController.predictionSource==="历史回放模型")?Theme.success:Theme.warning}ColumnLayout{Layout.fillWidth:true;Text{text:adminController.predictionStatus;color:Theme.textPrimary;font.pixelSize:Theme.fontBody}Text{text:"数据更新时间："+(adminController.predictionUpdatedAt||"—")+" · 历史回放不代表当前实时需求 · 模型离线时使用最近 24 小时业务订单估算";color:Theme.textMuted;font.pixelSize:Theme.fontCaption}}}}
        RowLayout{Layout.fillWidth:true;spacing:12
            MetricCard{Layout.fillWidth:true;label:"未来 1 小时累计";value:adminController.predictionLoad1;trend:adminController.predictionSource.indexOf("模型")>=0?"模型预测":"历史数据估算"}
            MetricCard{Layout.fillWidth:true;label:"未来 6 小时累计";value:adminController.predictionLoad6;trend:adminController.predictionSource.indexOf("模型")>=0?"模型预测":"历史数据估算";tone:"#9B87F5"}
            MetricCard{Layout.fillWidth:true;label:"未来 24 小时累计";value:adminController.predictionLoad24;trend:adminController.predictionSource.indexOf("模型")>=0?"模型预测":"历史数据估算";tone:Theme.warning}
            MetricCard{Layout.fillWidth:true;label:"当前预测方法";value:adminController.predictionMethod;trend:adminController.predictionConfidence==="—"?"未提供概率区间":adminController.predictionConfidence+"% 置信水平";tone:Theme.success}
        }
        DataTable{Layout.fillWidth:true;Layout.fillHeight:true;tableModel:adminController.predictionsModel;columns:[{title:"站点 / 区域",role:"station_name",width:260},{title:"1 小时累计",role:"h1",width:150,align:"right"},{title:"6 小时累计",role:"h6",width:150,align:"right"},{title:"24 小时累计",role:"h24",width:160,align:"right"},{title:"预计空闲桩",role:"free",width:135,align:"right"},{title:"高峰与风险",role:"risk",width:200}]}
        PanelCard{Layout.fillWidth:true;Layout.preferredHeight:116;ColumnLayout{anchors.fill:parent;anchors.margins:16;spacing:5;Text{text:"模型与数据口径";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}Text{Layout.fillWidth:true;text:"模型："+adminController.predictionModelName+" · "+adminController.predictionDataScope;color:Theme.textSecondary;font.pixelSize:Theme.fontBody;elide:Text.ElideRight}Text{Layout.fillWidth:true;text:"限制："+adminController.predictionCaveat;color:Theme.textMuted;font.pixelSize:Theme.fontCaption;wrapMode:Text.Wrap}}}
    }
}
