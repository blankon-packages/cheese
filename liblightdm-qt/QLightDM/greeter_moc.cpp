/****************************************************************************
** Meta object code from reading C++ file 'greeter.h'
**
** Created: Wed Jul 13 11:10:38 2011
**      by: The Qt Meta Object Compiler version 62 (Qt 4.7.3)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "greeter.h"
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'greeter.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 62
#error "This file was generated using the moc from 4.7.3. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
static const uint qt_meta_data_QLightDM__Greeter[] = {

 // content:
       5,       // revision
       0,       // classname
       0,    0, // classinfo
      21,   14, // methods
       5,  119, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       7,       // signalCount

 // signals: signature, parameters, type, tag, flags
      19,   18,   18,   18, 0x05,
      38,   31,   18,   18, 0x05,
      66,   58,   18,   18, 0x05,
      87,   58,   18,   18, 0x05,
     122,  106,   18,   18, 0x05,
     160,  151,   18,   18, 0x05,
     180,   18,   18,   18, 0x05,

 // slots: signature, parameters, type, tag, flags
     187,   18,   18,   18, 0x0a,
     197,   18,   18,   18, 0x0a,
     209,   18,   18,   18, 0x0a,
     220,   18,   18,   18, 0x0a,
     230,   18,   18,   18, 0x0a,
     248,   18,   18,   18, 0x0a,
     267,  151,   18,   18, 0x0a,
     282,   18,   18,   18, 0x0a,
     306,  297,   18,   18, 0x0a,
     323,   18,   18,   18, 0x0a,
     354,  346,   18,   18, 0x0a,
     376,   18,   18,   18, 0x2a,
     394,  391,   18,   18, 0x08,

 // methods: signature, parameters, type, tag, flags
     420,  415,  406,   18, 0x02,

 // properties: name, type, flags
     446,  441, 0x01095001,
     457,  441, 0x01095001,
     470,  441, 0x01095001,
     482,  441, 0x01095001,
     501,  493, 0x0a095401,

       0        // eod
};

static const char qt_meta_stringdata_QLightDM__Greeter[] = {
    "QLightDM::Greeter\0\0connected()\0prompt\0"
    "showPrompt(QString)\0message\0"
    "showMessage(QString)\0showError(QString)\0"
    "isAuthenticated\0authenticationComplete(bool)\0"
    "username\0timedLogin(QString)\0quit()\0"
    "suspend()\0hibernate()\0shutdown()\0"
    "restart()\0connectToServer()\0"
    "cancelTimedLogin()\0login(QString)\0"
    "loginAsGuest()\0response\0respond(QString)\0"
    "cancelAuthentication()\0session\0"
    "startSession(QString)\0startSession()\0"
    "fd\0onRead(int)\0QVariant\0name\0"
    "getProperty(QString)\0bool\0canSuspend\0"
    "canHibernate\0canShutdown\0canRestart\0"
    "QString\0hostname\0"
};

const QMetaObject QLightDM::Greeter::staticMetaObject = {
    { &QObject::staticMetaObject, qt_meta_stringdata_QLightDM__Greeter,
      qt_meta_data_QLightDM__Greeter, 0 }
};

#ifdef Q_NO_DATA_RELOCATION
const QMetaObject &QLightDM::Greeter::getStaticMetaObject() { return staticMetaObject; }
#endif //Q_NO_DATA_RELOCATION

const QMetaObject *QLightDM::Greeter::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->metaObject : &staticMetaObject;
}

void *QLightDM::Greeter::qt_metacast(const char *_clname)
{
    if (!_clname) return 0;
    if (!strcmp(_clname, qt_meta_stringdata_QLightDM__Greeter))
        return static_cast<void*>(const_cast< Greeter*>(this));
    return QObject::qt_metacast(_clname);
}

int QLightDM::Greeter::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: connected(); break;
        case 1: showPrompt((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 2: showMessage((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 3: showError((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 4: authenticationComplete((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 5: timedLogin((*reinterpret_cast< QString(*)>(_a[1]))); break;
        case 6: quit(); break;
        case 7: suspend(); break;
        case 8: hibernate(); break;
        case 9: shutdown(); break;
        case 10: restart(); break;
        case 11: connectToServer(); break;
        case 12: cancelTimedLogin(); break;
        case 13: login((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 14: loginAsGuest(); break;
        case 15: respond((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 16: cancelAuthentication(); break;
        case 17: startSession((*reinterpret_cast< const QString(*)>(_a[1]))); break;
        case 18: startSession(); break;
        case 19: onRead((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 20: { QVariant _r = getProperty((*reinterpret_cast< const QString(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< QVariant*>(_a[0]) = _r; }  break;
        default: ;
        }
        _id -= 21;
    }
#ifndef QT_NO_PROPERTIES
      else if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast< bool*>(_v) = canSuspend(); break;
        case 1: *reinterpret_cast< bool*>(_v) = canHibernate(); break;
        case 2: *reinterpret_cast< bool*>(_v) = canShutdown(); break;
        case 3: *reinterpret_cast< bool*>(_v) = canRestart(); break;
        case 4: *reinterpret_cast< QString*>(_v) = hostname(); break;
        }
        _id -= 5;
    } else if (_c == QMetaObject::WriteProperty) {
        _id -= 5;
    } else if (_c == QMetaObject::ResetProperty) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyDesignable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyScriptable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyStored) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyEditable) {
        _id -= 5;
    } else if (_c == QMetaObject::QueryPropertyUser) {
        _id -= 5;
    }
#endif // QT_NO_PROPERTIES
    return _id;
}

// SIGNAL 0
void QLightDM::Greeter::connected()
{
    QMetaObject::activate(this, &staticMetaObject, 0, 0);
}

// SIGNAL 1
void QLightDM::Greeter::showPrompt(QString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void QLightDM::Greeter::showMessage(QString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 2, _a);
}

// SIGNAL 3
void QLightDM::Greeter::showError(QString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 3, _a);
}

// SIGNAL 4
void QLightDM::Greeter::authenticationComplete(bool _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 4, _a);
}

// SIGNAL 5
void QLightDM::Greeter::timedLogin(QString _t1)
{
    void *_a[] = { 0, const_cast<void*>(reinterpret_cast<const void*>(&_t1)) };
    QMetaObject::activate(this, &staticMetaObject, 5, _a);
}

// SIGNAL 6
void QLightDM::Greeter::quit()
{
    QMetaObject::activate(this, &staticMetaObject, 6, 0);
}
QT_END_MOC_NAMESPACE
