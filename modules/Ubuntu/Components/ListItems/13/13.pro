TEMPLATE = aux

CONFIG+=ubuntu_qml_module

uri = Ubuntu.Components.ListItems.13
installPath = $$[QT_INSTALL_QML]/$$replace(uri, \\., /)

qml_files.installPath = $$installPath
qml_files.files = *.qml

# javascript files
js_files.installPath = $$installPath
js_files.files = *.js

UBUNTU_QML_MODULE_FILES += qml_files js_files