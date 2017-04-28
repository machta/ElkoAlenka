import QtQuick 2.7
import QtQuick.Controls 2.0



ImageManagerForm {

    function getCheckedImages() {
        var sourceArray = []
        for (var k = 0; k < swipe.count; k++) {
            for (var i = 0; i < swipe.itemAt(k).images.count; i++) {
                if (swipe.itemAt(k).images.itemAt(i).checkbox.checked) {
                    sourceArray.push(swipe.itemAt(k).images.itemAt(i).source)
                }
            }
        }
        return sourceArray
    }

    function confirm() {
        var checkedImages = getCheckedImages()
        console.log("User chose " + checkedImages.length + " image(s): " + checkedImages.toString())
        window.images = checkedImages
        window.changePage("Electrode Manager", "qrc:/pages/ElectrodeManager.qml", 2)
    }
}
