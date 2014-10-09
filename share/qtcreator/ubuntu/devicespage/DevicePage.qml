import QtQuick 2.0
import QtQuick.Layouts 1.0
import QtQuick.Controls 1.0 as Controls

import Ubuntu.Components 0.1
import Ubuntu.Components.ListItems 0.1 as ListItem
import Ubuntu.DevicesModel 0.1
import Ubuntu.Components.Popups 0.1

Page {
    id: devicePage
    flickable: null

    Item {
        anchors.fill: parent
        visible: devicesModel.busy

        Column {
            anchors.centerIn: parent
            spacing: units.gu(1)

            ActivityIndicator{
                anchors.horizontalCenter: parent.horizontalCenter
                running: devicesModel.busy
            }
            Label {
                text: i18n.tr("There is currently a process running in the background, please check the logs for details")
                fontSize: "large"
                anchors.left: parent.left
            }
            Button {
                visible: devicesModel.cancellable
                anchors.horizontalCenter: parent.horizontalCenter
                color: UbuntuColors.warmGrey
                text: "Cancel"
                onClicked: devicesModel.cancel()
            }
        }
    }

    Controls.SplitView {
        orientation: Qt.Horizontal
        anchors.fill: parent
        visible: !devicesModel.busy
        Controls.SplitView {
            orientation: Qt.Vertical
            width: 200
            Layout.minimumWidth: 200
            Layout.maximumWidth: 400

            Controls.ScrollView {
                Layout.fillHeight: true
                UbuntuListView {
                    id: devicesList
                    objectName: "devicesList"
                    model: devicesModel
                    currentIndex: 0
                    delegate: ListItem.Standard {
                        id: delegate
                        text: display
                        selected: devicesList.currentIndex == index
                        onClicked: devicesList.currentIndex = index
                        property alias editor: editor

                        TextField{
                            id: editor
                            anchors.fill: parent
                            visible: false

                            property bool changed: false
                            Keys.onEscapePressed: {
                                close();
                            }
                            Keys.onTabPressed: {
                                commit();
                                devicesList.incrementCurrentIndex();
                            }
                            Keys.onReturnPressed: {
                                commit();
                            }

                            onActiveFocusChanged: {
                                if(!activeFocus)
                                    close();
                            }

                            onTextChanged: {
                                changed = true;
                            }

                            function open (){
                                visible = true;
                                forceActiveFocus();
                                text = display;
                                changed = false;
                            }

                            function close (){
                                changed = false;
                                visible = false;
                            }

                            function commit (){
                                if(changed)
                                    edit = text;
                                close();
                            }

                            InverseMouseArea {
                                enabled: parent.visible
                                anchors.fill: parent
                                topmostItem: true
                                acceptedButtons: Qt.AllButtons
                                onPressed: parent.close()
                            }
                        }

                        Connections{
                            target: delegate.__mouseArea
                            onDoubleClicked: {
                                editor.open();
                            }
                        }

                    }
                    onCurrentIndexChanged: deviceMode.deviceSelected(currentIndex)
                }
            }
            Controls.ToolBar {
                Layout.fillWidth: true
                Layout.minimumHeight: units.gu(5)
                Layout.maximumHeight: units.gu(5)
                Row{
                    anchors.fill: parent
                    spacing: units.gu(2)
                    Controls.ToolButton {
                        text: i18n.tr("Refresh devices")
                        tooltip: text
                        iconSource: "qrc:/ubuntu/images/reload.svg"
                        onClicked: devicesModel.refresh()
                    }
                    Controls.ToolButton {
                        text: i18n.tr("Add Emulator")
                        tooltip: text
                        iconSource: "qrc:/ubuntu/images/list-add.svg"
                        onClicked: PopupUtils.open(resourceRoot+"/NewEmulatorDialog.qml",devicePage);
                    }

                    Connections{
                        target: deviceMode
                        onOpenAddEmulatorDialog: PopupUtils.open(resourceRoot+"/NewEmulatorDialog.qml",devicePage);
                    }
                }
            }
        }

        Item {
            id: centerItem
            Layout.minimumWidth: 400
            Layout.fillWidth: true

            Repeater {
                property int currentIndex: devicesList.currentIndex
                model: devicesModel

                Rectangle {
                    id: deviceItemView
                    property bool deviceConnected: connectionState === DeviceConnectionState.ReadyToUse || connectionState === DeviceConnectionState.Connected
                    property bool deviceBusy: (detectionState != DeviceDetectionState.Done && detectionState != DeviceDetectionState.NotStarted)
                    property bool deviceBooting: detectionState === DeviceDetectionState.Booting || detectionState === DeviceDetectionState.WaitForEmulator
                    property bool detectionError: detectionState === DeviceDetectionState.Error
                    anchors.fill: parent

                    color: Qt.rgba(0.0, 0.0, 0.0, 0.01)
                    visible: index == devicesList.currentIndex


                    Controls.ToolBar {
                        id: emulatorToolBar
                        height: visible ? units.gu(5) : 0
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        visible: machineType === DeviceMachineType.Emulator
                        Row{
                            anchors.fill: parent
                            spacing: units.gu(2)
                            Controls.ToolButton {
                                text: i18n.tr("Run Emulator")
                                tooltip: text
                                iconSource: "qrc:/projectexplorer/images/run.png"
                                onClicked: devicesModel.startEmulator(emulatorImageName)
                                visible: connectionState === DeviceConnectionState.Disconnected
                            }
                            Controls.ToolButton {
                                text: i18n.tr("Stop Emulator")
                                tooltip: text
                                iconSource: "qrc:/projectexplorer/images/stop.png"
                                onClicked: devicesModel.stopEmulator(emulatorImageName)
                                visible: connectionState !== DeviceConnectionState.Disconnected
                            }
                            Controls.ToolButton {
                                text: i18n.tr("Delete Emulator")
                                tooltip: text
                                iconSource: "qrc:/core/images/clear.png"
                                onClicked: PopupUtils.open(resourceRoot+"/DeleteEmulatorDialog.qml",devicePage, {"emulatorImageName": emulatorImageName})
                            }
                        }
                    }

                    ScrollableView {
                        id: deviceView
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: emulatorToolBar.bottom
                        anchors.bottom: parent.bottom
                        clip: true

                        ListItem.Empty {
                            divider.visible: false
                            visible: detectionError
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: units.gu(2)
                                anchors.rightMargin: units.gu(4)
                                Icon {
                                    id: errorIcon
                                    anchors.left: parent.left
                                    name: "security-alert"
                                    height:parent.height - units.gu(2)
                                    width: height
                                }
                                Label {
                                    id: errorText
                                    text: i18n.tr("There was a error in the device detection, check the log for details.")
                                    fontSize: "large"
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Button {
                                    id: deviceRedetectButton
                                    text: "Redetect"
                                    onClicked: devicesModel.triggerRedetect(deviceId)
                                }
                            }
                        }

                        ListItem.Empty {
                            divider.visible: false
                            visible: deviceItemView.deviceBooting
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: units.gu(2)
                                anchors.rightMargin: units.gu(4)
                                ActivityIndicator {
                                    running: deviceItemView.deviceBooting
                                    height:parent.height - units.gu(2)
                                    width: height
                                }
                                Label {
                                    text: i18n.tr("The device is currently booting, if this text is still shown after the device has booted, press the refresh button.")
                                    fontSize: "large"
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Button {
                                    text: "Redetect"
                                    onClicked: devicesModel.triggerRedetect(deviceId)
                                }
                            }
                        }

                        SectionItem {
                            title: deviceItemView.deviceConnected ? "Device Status: "+detectionStateString : "Device Status: Disconnected"
                            expanded: true

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right

                                ListItem.SingleValue {
                                    visible: deviceItemView.deviceConnected || machineType !== DeviceMachineType.Emulator
                                    text:i18n.tr("Serial ID")
                                    value: serial
                                }
                                ListItem.SingleValue {
                                    text: i18n.tr("Ubuntu version")
                                    value: emuUbuntuVersion
                                    visible: machineType === DeviceMachineType.Emulator
                                }
                                ListItem.SingleValue {
                                    text: i18n.tr("Device version")
                                    value: emuDeviceVersion
                                    visible: machineType === DeviceMachineType.Emulator
                                }
                                ListItem.SingleValue {
                                    text: i18n.tr("Image version")
                                    value: emuImageVersion
                                    visible: machineType === DeviceMachineType.Emulator
                                }
                                ListItem.Standard {
                                    //show this listitem only when device is not connected
                                    visible: machineType === DeviceMachineType.Emulator && !deviceItemView.deviceConnected
                                    text: "Scale"
                                    control: Controls.ComboBox {
                                        id: emulatorScaleComboBox
                                        model: ["1.0", "0.9", "0.8", "0.7", "0.6","0.5", "0.4", "0.3", "0.2","0.1"]
                                        currentIndex: {
                                            var idx = find(emulatorScaleFactor);
                                            return idx >= 0 ? idx : 0;
                                        }
                                        onActivated: {
                                            emulatorScaleFactor = textAt(index);
                                        }
                                    }
                                }

                                ListItem.Standard {
                                    //show this listitem only when device is not connected
                                    visible: machineType === DeviceMachineType.Emulator && !deviceItemView.deviceConnected
                                    text: "Memory"
                                    control: Controls.ComboBox {
                                        id: emulatorMemoryComboBox
                                        model: ["512", "768", "1024"]

                                        currentIndex: {
                                            var idx = find(emulatorMemorySetting);
                                            return idx >= 0 ? idx : 0;
                                        }
                                        onActivated: {
                                            emulatorMemorySetting = textAt(index);
                                        }
                                    }
                                }
                                ListItem.SingleValue {
                                    text:i18n.tr("Device")
                                    value: deviceInfo
                                    visible: deviceItemView.deviceConnected
                                }
                                ListItem.SingleValue {
                                    text:i18n.tr("Model")
                                    value: modelInfo
                                    visible: deviceItemView.deviceConnected
                                }
                                ListItem.SingleValue {
                                    text:i18n.tr("Product")
                                    value: productInfo
                                    visible: deviceItemView.deviceConnected
                                }

                                FeatureStateItem {
                                    text: "Has network connection"
                                    input: hasNetworkConnection
                                    inputRole: "hasNetworkConnection"
                                    checkable: hasNetworkConnection == FeatureState.NotAvailable && !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                    visible: deviceItemView.deviceConnected
                                }
                                FeatureStateItem {
                                    text: "Has devloper mode enabled"
                                    input: developerModeEnabled
                                    inputRole: "developerModeEnabled"
                                    checkable: !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                    visible: deviceItemView.deviceConnected
                                }
                                /*
                                FeatureStateItem {
                                    text: "Has writeable image"
                                    input: hasWriteableImage
                                    inputRole: "hasWriteableImage"
                                    checkable: false
                                    visible: deviceItemView.deviceConnected
                                }
                                */
                            }
                        }

                        SectionItem {
                            title: "Kits"
                            expanded: true

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right

                                Repeater {
                                    model: kits
                                    delegate: ListItem.Standard {
                                        text: modelData.displayName
                                        Layout.fillWidth: true
                                        control: Button{
                                            text: "Remove"
                                            enabled: !deviceItemView.deviceBusy
                                            onClicked: devicesModel.triggerKitRemove(deviceId,modelData.id)
                                        }
                                    }
                                }

                                Item {
                                    clip: true
                                    visible: kits.length === 0
                                    height: label.contentHeight + units.gu(15)
                                    width: parent.width
                                    Label {
                                        id:label
                                        anchors.centerIn: parent
                                        anchors.bottom: button.top
                                        fontSize: "large"
                                        text: "There is currently no Kit defined for your device.\n In order to use the device in your Projects,\n you can either add a existing Kit "
                                              +"\nor let Qt Creator autocreate one for you."
                                    }
                                    Button {
                                        id: button
                                        anchors.left: label.left
                                        anchors.right: label.right
                                        anchors.top: label.bottom
                                        anchors.bottom: parent.bottom
                                        anchors.topMargin: units.gu(2)
                                        text: "Autocreate"
                                        enabled: !deviceItemView.deviceBusy
                                        onClicked: devicesModel.triggerKitAutocreate(deviceId)
                                    }
                                }


                            }
                        }

                        SectionItem {
                            title: "Control"
                            visible: deviceItemView.deviceConnected

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right

                                ListItem.Standard {
                                    text:"Clone time config from Host to Device"
                                    control: Button{
                                        text: "Execute"
                                        enabled: !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                        onClicked: devicesModel.triggerCloneTimeConfig(deviceId)
                                    }
                                }
                                ListItem.Standard {
                                    text:"Enable port forwarding"
                                    control: Button{
                                        text: "Execute"
                                        enabled: !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                        onClicked: devicesModel.triggerPortForwarding(deviceId)
                                    }
                                }
                                ListItem.Standard {
                                    text:"Setup public key authentication"
                                    control: Button{
                                        text: "Execute"
                                        enabled: !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                        onClicked: devicesModel.triggerSSHSetup(deviceId)
                                    }
                                }
                                ListItem.Standard {
                                    text:"Open SSH connection to the device"
                                    control: Button{
                                        text: "Execute"
                                        enabled: !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                        onClicked: devicesModel.triggerSSHConnection(deviceId)
                                    }
                                }
                                ListItem.Standard {
                                    text:"Reboot"
                                    control: Button{
                                        text: "Execute"
                                        enabled: !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                        onClicked: devicesModel.triggerReboot(deviceId)
                                    }
                                }
                                ListItem.Standard {
                                    text:"Reboot to bootloader"
                                    control: Button{
                                        text: "Execute"
                                        enabled: !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                        onClicked: devicesModel.triggerRebootBootloader(deviceId)
                                    }
                                }
                                ListItem.Standard {
                                    text:"Reboot to recovery"
                                    control: Button{
                                        text: "Execute"
                                        enabled: !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                        onClicked: devicesModel.triggerRebootRecovery(deviceId)
                                    }
                                }
                                ListItem.Standard {
                                    text:"Shutdown"
                                    control: Button{
                                        text: "Execute"
                                        enabled: !deviceItemView.deviceBusy && !deviceItemView.detectionError
                                        onClicked: devicesModel.triggerShutdown(deviceId)
                                    }
                                }
                            }
                        }

                        SectionItem {
                            title: "Log"
                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                TextArea {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    height: units.gu(60)
                                    highlighted: true

                                    readOnly: true
                                    text: deviceLog
                                    textFormat: TextEdit.AutoText
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

