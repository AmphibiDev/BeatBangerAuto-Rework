import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root

    visible: true
    width: 340
    height: 94
    minimumWidth: width
    maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    title: "BeatBangerAuto Rework"
    color: "black"
    flags: Qt.Window | Qt.WindowTitleHint | Qt.WindowCloseButtonHint | Qt.WindowMinimizeButtonHint

    RowLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 15

        StatusPanel {
            Layout.fillWidth: true
        }

        ControlButton {
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64
        }
    }

    Timer {
        id: cooldownTimer
        interval: 500
        repeat: false
    }

    component StatusPanel: Column {
        spacing: 2

        Text {
            width: parent.width
            text: scanner ? scanner.statusText : "Made by Amphibi"
            font {
                pixelSize: 18
                bold: true
            }
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            color: "white"
            wrapMode: Text.Wrap
        }

        Text {
            width: parent.width
            text: scanner ? ("Game Version: " + scanner.gameVersion) : ""
            font {
                pixelSize: 12
                bold: false
            }
            horizontalAlignment: Text.AlignLeft
            verticalAlignment: Text.AlignVCenter
            color: "#888888"
            wrapMode: Text.Wrap
        }

        ConnectionStatus {
            width: parent.width
        }
    }

    component ConnectionStatus: Text {
        text: scanner ? scanner.connectionStatus : ""
        font {
            pixelSize: 12
            bold: false
        }
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        color: "#888888"
        wrapMode: Text.Wrap
        visible: opacity > 0

        property real targetOpacity: 0.0
        opacity: targetOpacity

        Behavior on opacity {
            NumberAnimation {
                duration: 400
                easing.type: Easing.InOutQuad
            }
        }

        Connections {
            target: scanner
            function onConnectionStatusChanged(status) {
                if (status !== "") {
                    targetOpacity = 1.0
                    fadeTimer.restart()
                } else {
                    targetOpacity = 0.0
                }
            }
        }

        Timer {
            id: fadeTimer
            interval: 3000
            repeat: false
            onTriggered: {
                parent.targetOpacity = 0.0
            }
        }
    }

    component ControlButton: Item {
        id: button

        property real scale: 1.0
        property bool isEnabled: !cooldownTimer.running

        transform: Scale {
            xScale: button.scale
            yScale: button.scale
            origin.x: button.width / 2
            origin.y: button.height / 2
        }

        Behavior on scale {
            NumberAnimation {
                duration: 100
                easing.type: Easing.OutQuart
            }
        }

        Image {
            anchors.fill: parent
            source: {
                if (scanner && (scanner.inAutoplay || scanner.scanning)) {
                    return "qrc:/resources/images/btn_on.png"
                }
                return "qrc:/resources/images/btn_off.png"
            }
            fillMode: Image.PreserveAspectFit
            opacity: button.isEnabled ? 1.0 : 0.6
        }

        MouseArea {
            anchors.fill: parent
            enabled: button.isEnabled

            onClicked: {
                if (scanner) {
                    scanner.toggle()
                    cooldownTimer.restart()
                }
            }

            onPressedChanged: {
                button.scale = pressed ? 0.85 : 1.0
            }
        }
    }
}
