import QtQuick
import QtQuick.Layouts
import Charging.UI
import "../components"
Item{id:root;Component.onCompleted:adminController.refreshPredictions()
    ColumnLayout{anchors.fill:parent;anchors.margins:24;spacing:16
        RowLayout{Layout.fillWidth:true;PageHeader{Layout.fillWidth:true;title:"智能预测";subtitle:"未来 1/6/24 小时负荷预测"}StatusBadge{status:adminController.predictionSource==="演示数据"?"warning":"active";label:adminController.predictionSource}AppButton{text:"刷新预测";variant:"secondary";onClicked:adminController.refreshPredictions()}}
        PanelCard{Layout.fillWidth:true;Layout.preferredHeight:70;RowLayout{anchors.fill:parent;anchors.margins:16;Rectangle{width:10;height:10;radius:5;color:adminController.predictionSource==="演示数据"?Theme.warning:Theme.success}ColumnLayout{Layout.fillWidth:true;Text{text:adminController.predictionStatus;color:Theme.textPrimary;font.pixelSize:Theme.fontBody}Text{text:"模型更新时间："+(adminController.predictionUpdatedAt||"—")+" · 历史实测与模型预测在正式服务响应中分别标记";color:Theme.textMuted;font.pixelSize:Theme.fontCaption}}}}
        RowLayout{Layout.fillWidth:true;spacing:12
            MetricCard{Layout.fillWidth:true;label:"未来 1 小时";value:adminController.predictionLoad1;trend:"模型预测"}
            MetricCard{Layout.fillWidth:true;label:"未来 6 小时";value:adminController.predictionLoad6;trend:"模型预测";tone:"#9B87F5"}
            MetricCard{Layout.fillWidth:true;label:"未来 24 小时";value:adminController.predictionLoad24;trend:"模型预测";tone:Theme.warning}
            MetricCard{Layout.fillWidth:true;label:"可信度";value:adminController.predictionConfidence;unit:adminController.predictionConfidence==="—"?"":"%";trend:"置信区间";tone:Theme.success}
        }
        DataTable{Layout.fillWidth:true;Layout.fillHeight:true;tableModel:adminController.predictionsModel;columns:[{title:"站点 / 区域",role:"station_name",width:260},{title:"1 小时预测",role:"h1",width:150,align:"right"},{title:"6 小时预测",role:"h6",width:150,align:"right"},{title:"24 小时预测",role:"h24",width:160,align:"right"},{title:"预计空闲桩",role:"free",width:135,align:"right"},{title:"高峰与风险",role:"risk",width:200}]}
        PanelCard{Layout.fillWidth:true;Layout.preferredHeight:90;ColumnLayout{anchors.fill:parent;anchors.margins:16;Text{text:"推荐调度建议";color:Theme.textPrimary;font.pixelSize:Theme.fontSubtitle;font.bold:true}Text{Layout.fillWidth:true;text:adminController.predictionSource==="演示数据"?"当前展示集中管理的演示预测值，仅用于验证界面。启动 ml/service.py 并准备模型产物后将自动切换为模型服务数据。":"根据预测服务结果，优先保障高峰站点可用容量，并安排低谷时段维护。";color:Theme.textSecondary;font.pixelSize:Theme.fontBody;wrapMode:Text.Wrap}}}
    }
}
