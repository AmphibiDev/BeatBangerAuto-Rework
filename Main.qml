import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: mainWindow
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
        spacing: 15
        anchors.fill: parent
        anchors.margins: 15

        Text {
            id: statusDisplay
            Layout.fillWidth: true
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

        Item {
            id: controlButton
            Layout.preferredWidth: 64
            Layout.preferredHeight: 64

            property real buttonScale: 1.0
            property bool enabled: !cooldownTimer.running

            transform: Scale {
                xScale: controlButton.buttonScale
                yScale: controlButton.buttonScale
                origin.x: controlButton.width / 2
                origin.y: controlButton.height / 2
            }

            Behavior on buttonScale {
                NumberAnimation {
                    duration: 100
                    easing.type: Easing.OutQuart
                }
            }

            Image {
                anchors.fill: parent
                source: getButtonImage()
                fillMode: Image.PreserveAspectFit
                opacity: controlButton.enabled ? 1.0 : 0.6

                function getButtonImage() {
                    if (scanner && (scanner.inAutoplay || scanner.scanning)) {
                        return "qrc:/resources/images/btn_on.png"
                    } else {
                        return "qrc:/resources/images/btn_off.png"
                    }
                }
            }

            MouseArea {
                anchors.fill: parent
                enabled: controlButton.enabled

                onClicked: {
                    if (scanner) {
                        scanner.toggle()
                        cooldownTimer.restart()
                    }
                }

                onPressedChanged: {
                    if (pressed) {
                        controlButton.buttonScale = 0.85
                    } else {
                        controlButton.buttonScale = 1.0
                    }
                }
            }
        }
    }

    Timer {
        id: cooldownTimer
        interval: 500
        repeat: false
    }
}
