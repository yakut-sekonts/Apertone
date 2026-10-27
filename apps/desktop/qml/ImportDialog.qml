import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
Dialog {
    id:dialog
    parent:Overlay.overlay;anchors.centerIn:parent;modal:true
    objectName:"library.importDialog"
    title:qsTranslate("Apertone","Import photos")
    width:Math.min(parent.width-40,680);height:Math.min(parent.height-40,800)
    property var sources:[]
    property url destination:""
    property url backup:""
    property string selectedMetadataPreset:""
    function metadataValues() {return {title:titleField.text,caption:caption.text,creator:creator.text,copyright:copyright.text,location:location.text,keywords:keywords.text}}
    function useMetadata(values) {titleField.text=values.title??"";caption.text=values.caption??"";creator.text=values.creator??"";copyright.text=values.copyright??"";location.text=values.location??"";keywords.text=values.keywords??""}
    FileDialog {id:files;title:qsTranslate("Apertone","Import photos");fileMode:FileDialog.OpenFiles;nameFilters:[qsTranslate("Apertone","Photos (*.nef *.NEF *.dng *.DNG *.cr2 *.CR2 *.cr3 *.CR3 *.arw *.ARW *.rw2 *.RW2 *.orf *.ORF *.raf *.RAF *.jpg *.JPG *.jpeg *.JPEG *.png *.PNG *.tif *.TIF *.tiff *.TIFF *.heic *.HEIC *.heif *.HEIF *.webp *.WEBP *.bmp *.BMP *.gif *.GIF *.avif *.AVIF)"),qsTranslate("Apertone","All files (*)")];onAccepted:dialog.sources=selectedFiles}
    FolderDialog {id:folder;title:qsTranslate("Apertone","Photo folder");onAccepted:dialog.sources=[selectedFolder]}
    FolderDialog {id:destinationPicker;title:qsTranslate("Apertone","Choose destination folder");onAccepted:dialog.destination=selectedFolder}
    FolderDialog {id:backupPicker;title:qsTranslate("Apertone","Second backup folder");onAccepted:dialog.backup=selectedFolder}
    ScrollView {
        id:scroll;anchors.fill:parent;clip:true;contentWidth:availableWidth
        ColumnLayout {
            width:scroll.availableWidth;spacing:10
            RowLayout {
                ApertoneButton {text:qsTranslate("Apertone","Choose files");onClicked:files.open()}
                ApertoneButton {text:qsTranslate("Apertone","Choose folder");onClicked:folder.open()}
            }
            Label {objectName:"library.importSources";text:dialog.sources.length?qsTranslate("Apertone","Sources: ")+dialog.sources.length+" · "+dialog.sources[0]:qsTranslate("Apertone","No files or folder selected");wrapMode:Text.WrapAnywhere;Layout.fillWidth:true}
            ApertoneComboBox {
                id:mode;objectName:"library.importMode";Layout.fillWidth:true
                model:[qsTranslate("Apertone","Add · keep originals in place"),qsTranslate("Apertone","Copy · copy into the selected folder"),qsTranslate("Apertone","Move · transfer originals after verification")]
            }
            RowLayout {
                visible:mode.currentIndex!==0;Layout.fillWidth:true
                ApertoneButton {text:qsTranslate("Apertone","Choose destination folder");onClicked:destinationPicker.open()}
                Label {text:dialog.destination.toString()||qsTranslate("Apertone","No folder selected");elide:Text.ElideMiddle;Layout.fillWidth:true}
            }
            CheckBox {id:dates;visible:mode.currentIndex!==0;text:qsTranslate("Apertone","Date folders · YYYY/MM/DD (UTC)")}
            ApertoneTextField {id:naming;visible:mode.currentIndex!==0;text:"{name}";maximumLength:230;Layout.fillWidth:true;Accessible.name:qsTranslate("Apertone","Filename template")}
            Label {visible:mode.currentIndex!==0;text:qsTranslate("Apertone","Filename tokens: {name}, {date}, {seq}. The original extension is kept. Missing capture dates use the import date.");wrapMode:Text.Wrap;Layout.fillWidth:true}
            CheckBox {id:recursive;objectName:"library.importRecursive";text:qsTranslate("Apertone","Include subfolders");checked:true}
            CheckBox {id:duplicates;objectName:"library.importDuplicates";text:qsTranslate("Apertone","Skip duplicates by SHA-256 and size");checked:true}
            RowLayout {
                Layout.fillWidth:true
                ApertoneButton {text:qsTranslate("Apertone","Second backup folder");onClicked:backupPicker.open()}
                Label {text:dialog.backup.toString()||qsTranslate("Apertone","None");elide:Text.ElideMiddle;Layout.fillWidth:true}
                ApertoneButton {text:qsTranslate("Apertone","Clear");enabled:dialog.backup.toString().length>0;onClicked:dialog.backup=""}
            }
            Label {text:qsTranslate("Apertone","Develop preset")}
            ApertoneComboBox {
                id:develop;Layout.fillWidth:true;textRole:"name";valueRole:"id"
                model:[{id:"",name:qsTranslate("Apertone","None")}].concat(presets.builtinItems).concat(presets.items)
            }
            Label {text:qsTranslate("Apertone","Metadata preset")}
            RowLayout {
                Layout.fillWidth:true
                ApertoneComboBox {
                    id:metadataPreset;Layout.fillWidth:true;textRole:"name"
                    model:[{name:qsTranslate("Apertone","None"),values:{}}].concat(library.importMetadataPresets)
                    onActivated:{dialog.selectedMetadataPreset=currentIndex>0?model[currentIndex].name:"";dialog.useMetadata(model[currentIndex].values);presetName.text=dialog.selectedMetadataPreset}
                }
                ApertoneButton {text:qsTranslate("Apertone","Delete");enabled:dialog.selectedMetadataPreset.length>0;onClicked:if(library.removeImportMetadataPreset(dialog.selectedMetadataPreset)){dialog.selectedMetadataPreset="";metadataPreset.currentIndex=0}}
            }
            ApertoneTextField {id:titleField;placeholderText:qsTranslate("Apertone","Title");maximumLength:1024;Layout.fillWidth:true}
            ApertoneTextField {id:caption;placeholderText:qsTranslate("Apertone","Caption");maximumLength:16384;Layout.fillWidth:true}
            ApertoneTextField {id:creator;placeholderText:qsTranslate("Apertone","Creator");maximumLength:1024;Layout.fillWidth:true}
            ApertoneTextField {id:copyright;placeholderText:qsTranslate("Apertone","Copyright");maximumLength:2048;Layout.fillWidth:true}
            ApertoneTextField {id:location;placeholderText:qsTranslate("Apertone","Location");maximumLength:2048;Layout.fillWidth:true}
            ApertoneTextField {id:keywords;placeholderText:qsTranslate("Apertone","Keywords, separated by commas");maximumLength:32768;Layout.fillWidth:true}
            Label {text:qsTranslate("Apertone","Empty metadata fields preserve existing values. Keywords are added without duplicates.");wrapMode:Text.Wrap;Layout.fillWidth:true}
            RowLayout {
                Layout.fillWidth:true
                ApertoneTextField {id:presetName;placeholderText:qsTranslate("Apertone","Metadata preset name");maximumLength:128;Layout.fillWidth:true}
                ApertoneButton {text:qsTranslate("Apertone","Save preset");enabled:presetName.text.trim().length>0;onClicked:library.saveImportMetadataPreset(presetName.text,dialog.metadataValues())}
            }
            Label {visible:mode.currentIndex===2;text:qsTranslate("Apertone","Move deletes each source only after the imported file, optional backup and catalog entry are complete. Duplicate sources are kept.");wrapMode:Text.Wrap;Layout.fillWidth:true}
            Label {visible:library.error.length>0;text:qsTranslate("Apertone",library.error);wrapMode:Text.Wrap;Layout.fillWidth:true;color:Theme.negative}
            Label {visible:presets.error.length>0;text:qsTranslate("Apertone",presets.error);wrapMode:Text.Wrap;Layout.fillWidth:true;color:Theme.negative}
        }
    }
    footer:DialogButtonBox {
        ApertoneButton {objectName:"library.importCancel";text:qsTranslate("Apertone","Cancel");onClicked:dialog.reject()}
        ApertoneButton {
            objectName:"library.importStart";text:qsTranslate("Apertone","Import photos")
            enabled:dialog.sources.length>0&&(mode.currentIndex===0||dialog.destination.toString().length>0)&&!library.working
            onClicked: {
                const preset=develop.currentValue?presets.importSnapshot(develop.currentValue):""
                if(develop.currentValue&&!preset)return
                if(library.importAdvanced(dialog.sources,{mode:mode.currentIndex,destination:dialog.destination,backup:dialog.backup,recursive:recursive.checked,skipDuplicates:duplicates.checked,dateFolders:mode.currentIndex!==0&&dates.checked,naming:mode.currentIndex===0?"{name}":naming.text,metadata:dialog.metadataValues()},preset))dialog.close()
            }
        }
    }
}
