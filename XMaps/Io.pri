win32 {
    CONFIG(debug, debug|release): universalOutPwd = $$shell_path($$OUT_PWD/debug)
    CONFIG(release, debug|release): universalOutPwd = $$shell_path($$OUT_PWD/release)
    universalConnector = $$escape_expand(\n)
}
else {
    unix:!android: universalOutPwd = $$OUT_PWD
    universalConnector = &&
}

defineTest(copy) {
    # Path
    source = $$shell_path($$PWD/$$1)
    destination = $$shell_path($$universalOutPwd/$$2)

    # Set
    !isEmpty(QMAKE_POST_LINK): QMAKE_POST_LINK += $$universalConnector
    exists($$1/*): QMAKE_POST_LINK += $$QMAKE_COPY_DIR
    else: QMAKE_POST_LINK += $$QMAKE_COPY
    QMAKE_POST_LINK += $$source $$destination

    # Execute
    export(QMAKE_POST_LINK)
}

defineTest(deleteFile) {
    # Path
    directory = $$shell_path($$universalOutPwd/$$1)

    # Set
    !isEmpty(QMAKE_POST_LINK): QMAKE_POST_LINK += $$universalConnector
    QMAKE_POST_LINK += $$QMAKE_DEL_FILE $$directory

    # Execute
    export(QMAKE_POST_LINK)
}
