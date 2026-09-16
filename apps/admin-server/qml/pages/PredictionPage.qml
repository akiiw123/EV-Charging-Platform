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
            MetricCard{Layout.fillWidth:true;label:"预测区间";value:adminController.predictionConfidence;unit:adminController.predictionConfidence==="—"?"":"%";trend:adminController.predictionConfidence==="—"?"当前模型未提供":"置信水平";tone:Theme.success}
        }
        DataTable{Layout.fillWidth:true;Layout.fillHeight:true;tableModel:adminController.predictionsModel;columns:[{title:"站点 / 区域",role:"station_name",width:260},{title:"1 小时累计",role:"h1",width:150,align:"right"},{title:"6 小时累计",role:"h6",width:150,align:"right"},{title:"24 小时累计",role:"h24",width:160,align:"right"},{title:"预计空闲桩",role:"free",width:135,align:"right"},{title:"高峰与风险",role:"risk",width:200}]}
        PanelCard{Layout.fillWidth:true;Layout.preferredHeight:90;ColumnLayout{anchors.fill:parent;anchors.margins:16;Text{text:"推荐调度建议";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}Text{Layout.fillWidth:true;text:adminController.predictionSource.indexOf("模型")>=0?"根据模型输出识别高负荷时段和容量风险；历史回放结果仅用于模型能力展示，不用于实时调度。":"当前使用平台真实订单与空闲桩做历史数据估算；启动 ml/service.py 并准备模型产物后会自动切换为模型预测。";color:Theme.textSecondary;font.pixelSize:Theme.fontBody;wrapMode:Text.Wrap}}}
    }
}
