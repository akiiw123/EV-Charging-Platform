import QtQuick

Canvas {
    id: icon
    property string name: "dashboard"
    property color strokeColor: Theme.textSecondary
    implicitWidth: 20; implicitHeight: 20
    onStrokeColorChanged: requestPaint()
    onNameChanged: requestPaint()
    onPaint: {
        var c = getContext("2d"); c.reset(); c.strokeStyle = strokeColor; c.fillStyle = "transparent"; c.lineWidth = 1.7; c.lineCap = "round"; c.lineJoin = "round";
        var w=width, h=height, x=w/2, y=h/2;
        c.beginPath();
        if (name === "dashboard") { c.rect(2,2,6,6); c.rect(12,2,6,6); c.rect(2,12,6,6); c.rect(12,12,6,6); }
        else if (name === "station") { c.moveTo(3,18); c.lineTo(3,7); c.lineTo(10,2); c.lineTo(17,7); c.lineTo(17,18); c.moveTo(1,18); c.lineTo(19,18); c.moveTo(7,10); c.lineTo(13,10); c.moveTo(7,14); c.lineTo(13,14); }
        else if (name === "pile") { c.roundedRect(5,2,10,16,2,2); c.moveTo(8,6); c.lineTo(12,6); c.moveTo(10,9); c.lineTo(8,12); c.lineTo(11,12); c.lineTo(9,15); }
        else if (name === "order") { c.rect(4,2,12,16); c.moveTo(7,6); c.lineTo(13,6); c.moveTo(7,10); c.lineTo(13,10); c.moveTo(7,14); c.lineTo(11,14); }
        else if (name === "user") { c.arc(10,6,3,0,Math.PI*2); c.moveTo(3,18); c.quadraticCurveTo(4,11,10,11); c.quadraticCurveTo(16,11,17,18); }
        else if (name === "chart") { c.moveTo(2,17); c.lineTo(2,3); c.moveTo(2,17); c.lineTo(18,17); c.moveTo(5,14); c.lineTo(9,9); c.lineTo(12,12); c.lineTo(18,5); }
        else if (name === "settings") { c.arc(10,10,3,0,Math.PI*2); c.arc(10,10,7,0,Math.PI*2); }
        else if (name === "logout") { c.moveTo(9,3); c.lineTo(3,3); c.lineTo(3,17); c.lineTo(9,17); c.moveTo(8,10); c.lineTo(18,10); c.moveTo(14,6); c.lineTo(18,10); c.lineTo(14,14); }
        else if (name === "menu") { c.moveTo(3,5); c.lineTo(17,5); c.moveTo(3,10); c.lineTo(17,10); c.moveTo(3,15); c.lineTo(17,15); }
        else { c.arc(10,10,7,0,Math.PI*2); }
        c.stroke();
    }
}
