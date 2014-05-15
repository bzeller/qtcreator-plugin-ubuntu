import QtQuick 2.0
import Ubuntu.Components 1.0
import Ubuntu.Components.ListItems 1.0 as ListItem
import Ubuntu.Components.Popups 1.0

Dialog {
    id: dialogue
    title: i18n.tr("Create emulator")
    text:  i18n.tr("Please select a name for the emulator")
    modal: true
    TextField {
        id: inputName
        placeholderText: i18n.tr("Emulator name")
        property string lastError
        property bool hasError
        onTextChanged: validate()
        Component.onCompleted: validate()
        function validate() {
            var result = devicesModel.validateEmulatorName(text);
            hasError   = !result.valid;
            lastError  = result.error;
        }
    }
    Label {
        horizontalAlignment: Text.AlignHCenter
        text: inputName.lastError
        color: "red"
        visible: inputName.hasError
    }

    ListItem.ItemSelector {
        id: arch
        model: [i18n.tr("i386"),
            i18n.tr("armhf")]
    }

    Button {
        text: "cancel"
        onClicked: PopupUtils.close(dialogue)
    }
    Button {
        text: "create"
        color: UbuntuColors.orange
        enabled: !inputName.hasError
        onClicked: {
            if(inputName.hasError)
                return;
            devicesModel.createEmulatorImage(inputName.text,arch.model[arch.selectedIndex]);
            PopupUtils.close(dialogue);
        }
    }
}

