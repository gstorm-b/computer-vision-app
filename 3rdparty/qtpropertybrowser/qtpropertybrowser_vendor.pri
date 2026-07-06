# Vendored Qt Solutions property browser, consumed as source by the root app.
# Do not modify vendored files for app features; app-side extensions live in
# src/ui/widgets/property_browser/.
# The legacy qtpropertybrowser.pri in this folder is part of the original
# Qt Solutions distribution (expects ../common.pri) and is not used.
# App code includes these headers as "qtpropertybrowser/<name>" via the
# INCLUDEPATH += 3rdparty entry in the root .pro.

SOURCES += \
    $$PWD/qtbuttonpropertybrowser.cpp \
    $$PWD/qteditorfactory.cpp \
    $$PWD/qtgroupboxpropertybrowser.cpp \
    $$PWD/qtpropertybrowser.cpp \
    $$PWD/qtpropertybrowserutils.cpp \
    $$PWD/qtpropertymanager.cpp \
    $$PWD/qttreepropertybrowser.cpp \
    $$PWD/qtvariantproperty.cpp

HEADERS += \
    $$PWD/QtAbstractEditorFactoryBase \
    $$PWD/QtAbstractPropertyBrowser \
    $$PWD/QtAbstractPropertyManager \
    $$PWD/QtBoolPropertyManager \
    $$PWD/QtBrowserItem \
    $$PWD/QtButtonPropertyBrowser \
    $$PWD/QtCharEditorFactory \
    $$PWD/QtCharPropertyManager \
    $$PWD/QtCheckBoxFactory \
    $$PWD/QtColorEditorFactory \
    $$PWD/QtColorPropertyManager \
    $$PWD/QtCursorEditorFactory \
    $$PWD/QtCursorPropertyManager \
    $$PWD/QtDateEditFactory \
    $$PWD/QtDatePropertyManager \
    $$PWD/QtDateTimeEditFactory \
    $$PWD/QtDateTimePropertyManager \
    $$PWD/QtDoublePropertyManager \
    $$PWD/QtDoubleSpinBoxFactory \
    $$PWD/QtEnumEditorFactory \
    $$PWD/QtEnumPropertyManager \
    $$PWD/QtFlagPropertyManager \
    $$PWD/QtFontEditorFactory \
    $$PWD/QtFontPropertyManager \
    $$PWD/QtGroupBoxPropertyBrowser \
    $$PWD/QtGroupPropertyManager \
    $$PWD/QtIntPropertyManager \
    $$PWD/QtKeySequenceEditorFactory \
    $$PWD/QtKeySequencePropertyManager \
    $$PWD/QtLineEditFactory \
    $$PWD/QtLocalePropertyManager \
    $$PWD/QtPointFPropertyManager \
    $$PWD/QtPointPropertyManager \
    $$PWD/QtProperty \
    $$PWD/QtRectFPropertyManager \
    $$PWD/QtRectPropertyManager \
    $$PWD/QtScrollBarFactory \
    $$PWD/QtSizeFPropertyManager \
    $$PWD/QtSizePolicyPropertyManager \
    $$PWD/QtSizePropertyManager \
    $$PWD/QtSliderFactory \
    $$PWD/QtSpinBoxFactory \
    $$PWD/QtStringPropertyManager \
    $$PWD/QtTimeEditFactory \
    $$PWD/QtTimePropertyManager \
    $$PWD/QtTreePropertyBrowser \
    $$PWD/QtVariantEditorFactory \
    $$PWD/QtVariantProperty \
    $$PWD/QtVariantPropertyManager \
    $$PWD/qtbuttonpropertybrowser.h \
    $$PWD/qteditorfactory.h \
    $$PWD/qtgroupboxpropertybrowser.h \
    $$PWD/qtpropertybrowser.h \
    $$PWD/qtpropertybrowserutils_p.h \
    $$PWD/qtpropertymanager.h \
    $$PWD/qttreepropertybrowser.h \
    $$PWD/qtvariantproperty.h

RESOURCES += \
    $$PWD/qtpropertybrowser.qrc

DISTFILES += \
    $$PWD/images/button-reset.ico \
    $$PWD/images/cursor-arrow.png \
    $$PWD/images/cursor-busy.png \
    $$PWD/images/cursor-closedhand.png \
    $$PWD/images/cursor-cross.png \
    $$PWD/images/cursor-forbidden.png \
    $$PWD/images/cursor-hand.png \
    $$PWD/images/cursor-hsplit.png \
    $$PWD/images/cursor-ibeam.png \
    $$PWD/images/cursor-openhand.png \
    $$PWD/images/cursor-sizeall.png \
    $$PWD/images/cursor-sizeb.png \
    $$PWD/images/cursor-sizef.png \
    $$PWD/images/cursor-sizeh.png \
    $$PWD/images/cursor-sizev.png \
    $$PWD/images/cursor-uparrow.png \
    $$PWD/images/cursor-vsplit.png \
    $$PWD/images/cursor-wait.png \
    $$PWD/images/cursor-whatsthis.png \
    $$PWD/qtpropertybrowser.pri
