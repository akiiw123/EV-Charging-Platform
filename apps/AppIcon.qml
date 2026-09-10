import QtQuick

// 简约线性图标:Canvas 矢量绘制,替代字符图标
// (字符图标依赖系统字体,不同环境渲染不一致,且辨识度低)
Canvas {
    id: root
    property string name: "bolt"          // home | bolt | person | pen | order
    property color iconColor: "#FFFFFF"
    width: 20; height: 20
    antialiasing: true
    onNameChanged: requestPaint()
    onIconColorChanged: requestPaint()
    onPaint: {
        var ctx = getContext("2d")
        ctx.reset()
        ctx.strokeStyle = root.iconColor
        ctx.fillStyle = root.iconColor
        ctx.lineWidth = 1.8
        ctx.lineCap = "round"
        ctx.lineJoin = "round"
        ctx.scale(width / 20, height / 20)
        if (name === "bolt") {            // 闪电
            ctx.beginPath()
            ctx.moveTo(11, 2); ctx.lineTo(5.5, 11); ctx.lineTo(9.5, 11)
            ctx.lineTo(8.5, 18); ctx.lineTo(14.5, 9); ctx.lineTo(10.5, 9)
            ctx.closePath(); ctx.stroke()
        } else if (name === "home") {     // 房子
            ctx.beginPath()
            ctx.moveTo(3.5, 9.5); ctx.lineTo(10, 3.5); ctx.lineTo(16.5, 9.5)
            ctx.moveTo(5.5, 8.5); ctx.lineTo(5.5, 16.5)
            ctx.lineTo(14.5, 16.5); ctx.lineTo(14.5, 8.5)
            ctx.stroke()
        } else if (name === "person") {   // 人
            ctx.beginPath()
            ctx.arc(10, 6.5, 3.2, 0, Math.PI * 2)
            ctx.moveTo(4, 17)
            ctx.bezierCurveTo(4.6, 12.6, 7, 11.4, 10, 11.4)
            ctx.bezierCurveTo(13, 11.4, 15.4, 12.6, 16, 17)
            ctx.stroke()
        } else if (name === "order") {    // 订单清单
            ctx.beginPath()
            ctx.moveTo(5, 4.5); ctx.lineTo(15, 4.5)
            ctx.moveTo(5, 9); ctx.lineTo(15, 9)
            ctx.moveTo(5, 13.5); ctx.lineTo(11, 13.5)
            ctx.stroke()
        } else if (name === "pen") {      // 铅笔
            ctx.beginPath()
            ctx.moveTo(4, 16); ctx.lineTo(5.2, 12.6); ctx.lineTo(13.4, 4.4)
            ctx.lineTo(15.6, 6.6); ctx.lineTo(7.4, 14.8); ctx.closePath()
            ctx.moveTo(12.3, 5.5); ctx.lineTo(14.5, 7.7)
            ctx.stroke()
        }
    }
}
