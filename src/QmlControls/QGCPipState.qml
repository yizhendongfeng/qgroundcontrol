/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

import QtQuick 2.12

// Used to manage state for itesm using with QGCPipOveral
Item {
    id:     _root
    state:  initState

    readonly property string initState:             "init"
    readonly property string pipState:              "pip"
    readonly property string fullState:             "full"
    readonly property string windowState:           "window"
    readonly property string windowClosingState:    "windowClosing"

    property var  pipOverlay            // QGCPipOverlay control
    property Item pipItem
    property Item fullItem
    property bool isDark:       true    // true: Use dark overlay visuals

    signal windowAboutToOpen    // Catch this signal to do something special prior to the item transition to windowed mode
    signal windowAboutToClose   // Catch this signal to do special processing prior to the item transition back to pip mode

    property var _clientControl: _root.parent

    states: [
        State {
            name: pipState
            StateChangeScript {
                script:  {
                    _clientControl.parent = pipItem
                    console.log("pipState pipState:", pipItem.width)
                }
            }

//            ParentChange {
//                target: _clientControl
//                parent: pipOverlay._currentItem
//            }
//            AnchorChanges {
//                target:         _clientControl
//                anchors.top:    pipItem.top
//                anchors.bottom: pipItem.bottom
//                anchors.left:   pipItem.left
//                anchors.right:  pipItem.right
//                anchors.top:    pipOverlay.top
//                anchors.bottom: pipOverlay.bottom
//                anchors.left:   pipOverlay.left
//                anchors.right:  pipOverlay.right
//            }

            PropertyChanges {
                target: _clientControl
                z:      pipOverlay.pipZOrder + 2
            }
        },
        State {
            name: fullState
            StateChangeScript {
                script: {
                    console.log("pipState fullState:",  pipOverlay.parent.width)
                    _clientControl.parent = pipOverlay.parent
                }
            }
//            ParentChange {
//                target: _clientControl
//                parent: pipOverlay.parent
//            }
//            AnchorChanges {
//                target:         _clientControl
//                anchors.top:    fullItem.top
//                anchors.bottom: fullItem.bottom
//                anchors.left:   fullItem.left
//                anchors.right:  fullItem.right
//                anchors.top:    pipOverlay.parent.top
//                anchors.bottom: pipOverlay.parent.bottom
//                anchors.left:   pipOverlay.parent.left
//                anchors.right:  pipOverlay.parent.right
//            }

//            PropertyChanges {
//                target: _clientControl
//                z:      pipOverlay.fullZOrder
//            }
        },
        State {
            name: windowState

            AnchorChanges {
                target:         _root.parent
                anchors.top:    pipOverlay._windowContentItem.top
                anchors.bottom: pipOverlay._windowContentItem.bottom
                anchors.left:   pipOverlay._windowContentItem.left
                anchors.right:  pipOverlay._windowContentItem.right
            }

            ParentChange {
                target: _root.parent
                parent: pipOverlay._windowContentItem
            }

            StateChangeScript {
                script: {
                    _root.windowAboutToOpen()
                    pipOverlay.showWindow()
                }
            }
        },
        State {
            name: windowClosingState

            ParentChange {
                target: _root.parent
                parent: pipOverlay.parent
            }
        }
    ]
}
