import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.0 as Controls

import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem
import Ubuntu.Components.Popups 0.1

import Ubuntu.DevicesModel 0.1

ColumnLayout {

    property alias memory: emulatorMemoryComboBox.currentText

    UbuntuListView {
        anchors.left: parent.left
        width: units.gu(50)
        Layout.fillHeight: true
        model: VisualItemModel {
            ListItem.SingleValue {
                text: i18n.tr("Ubuntu version")
                value: emuUbuntuVersion
            }
            ListItem.SingleValue {
                text: i18n.tr("Device version")
                value: emuDeviceVersion
            }
            ListItem.SingleValue {
                text: i18n.tr("Image version")
                value: emuImageVersion
            }
            ListItem.Standard {
                //show this listitem only when device is not connected
                visible: connectionState !== DeviceConnectionState.ReadyToUse && connectionState !== DeviceConnectionState.Connected
                text: "Memory"
                control: Controls.ComboBox {
                    id: emulatorMemoryComboBox
                    model: ["512", "768", "1024"]
                }
            }
        }
    }
}
