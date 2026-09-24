/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#include "SerialLink.h"
#include "QGCLoggingCategory.h"
#include "QGCSerialPortInfo.h"
#include <QtCore/QSettings>
#include <QtCore/QThread>
#include <QtCore/QTimer>

QGC_LOGGING_CATEGORY(SerialLinkLog, "qgc.comms.seriallink")

namespace {
    constexpr int CONNECT_TIMEOUT_MS = 1000;
    constexpr int DISCONNECT_TIMEOUT_MS = 3000;
    constexpr int READ_TIMEOUT_MS = 100;

    /// open() 失败后的重试次数（每秒一次）。Windows 上设备刚枚举出来、或刚被上一个持有者
    /// 放掉时，open 会短暂返回 ERROR_ACCESS_DENIED，界面上就是"拒绝访问"。
    constexpr int OPEN_RETRY_ATTEMPTS = 5;

    /// "连着却收不到数据"的判定时间。取 6s 是为了让飞控刚插上时的 bootloader 阶段过去
    /// （同一个考虑见 LinkManager::_autoconnectConnectDelayMSecs）；正常情况下 MAVLink
    /// 心跳 1Hz，活着的链路不会静默这么久。
    constexpr int SILENCE_TIMEOUT_MS = 6000;

    /// 收不到数据时重开端口的上限次数
    constexpr int NO_DATA_REOPEN_ATTEMPTS = 3;
}

/*===========================================================================*/

SerialConfiguration::SerialConfiguration(const QString &name, QObject *parent)
    : LinkConfiguration(name, parent)
{
    // qCDebug(SerialLinkLog) << this;
}

SerialConfiguration::SerialConfiguration(const SerialConfiguration *source, QObject *parent)
    : LinkConfiguration(source, parent)
{
    // qCDebug(SerialLinkLog) << this;

    SerialConfiguration::copyFrom(source);
}

SerialConfiguration::~SerialConfiguration()
{
    // qCDebug(SerialLinkLog) << this;
}

void SerialConfiguration::setPortName(const QString &name)
{
    const QString portName = name.trimmed();
    if (portName.isEmpty()) {
        return;
    }

    if (portName != _portName) {
        _portName = portName;
        emit portNameChanged();
    }

    const QString portDisplayName = cleanPortDisplayName(portName);
    setPortDisplayName(portDisplayName);
}

void SerialConfiguration::copyFrom(const LinkConfiguration *source)
{
    Q_ASSERT(source);
    LinkConfiguration::copyFrom(source);

    const SerialConfiguration* const serialSource = qobject_cast<const SerialConfiguration*>(source);
    Q_ASSERT(serialSource);

    setBaud(serialSource->baud());
    setDataBits(serialSource->dataBits());
    setFlowControl(serialSource->flowControl());
    setStopBits(serialSource->stopBits());
    setParity(serialSource->parity());
    setPortName(serialSource->portName());
    setPortDisplayName(serialSource->portDisplayName());
    setUsbDirect(serialSource->usbDirect());
}

void SerialConfiguration::loadSettings(QSettings &settings, const QString &root)
{
    settings.beginGroup(root);

    setBaud(settings.value("baud", _baud).toInt());
    setDataBits(static_cast<QSerialPort::DataBits>(settings.value("dataBits", _dataBits).toInt()));
    setFlowControl(static_cast<QSerialPort::FlowControl>(settings.value("flowControl", _flowControl).toInt()));
    setStopBits(static_cast<QSerialPort::StopBits>(settings.value("stopBits", _stopBits).toInt()));
    setParity(static_cast<QSerialPort::Parity>(settings.value("parity", _parity).toInt()));
    setPortName(settings.value("portName", _portName).toString());
    setPortDisplayName(settings.value("portDisplayName", _portDisplayName).toString());

    settings.endGroup();
}

void SerialConfiguration::saveSettings(QSettings &settings, const QString &root) const
{
    settings.beginGroup(root);

    settings.setValue("baud", _baud);
    settings.setValue("dataBits", _dataBits);
    settings.setValue("flowControl", _flowControl);
    settings.setValue("stopBits", _stopBits);
    settings.setValue("parity", _parity);
    settings.setValue("portName", _portName);
    settings.setValue("portDisplayName", _portDisplayName);

    settings.endGroup();
}

QStringList SerialConfiguration::supportedBaudRates()
{
    QStringList supportBaudRateStrings;

    const QList<qint32> rates = QSerialPortInfo::standardBaudRates();
    for (qint32 rate : rates) {
        supportBaudRateStrings.append(QString::number(rate));
    }

    return supportBaudRateStrings;
}

QString SerialConfiguration::cleanPortDisplayName(const QString &name)
{
    const QList<QSerialPortInfo> availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &portInfo : availablePorts) {
        if (portInfo.systemLocation() == name) {
            return portInfo.portName();
        }
    }

    return QString();
}

/*===========================================================================*/

SerialWorker::SerialWorker(const SerialConfiguration *config, QObject *parent)
    : QObject(parent)
    , _serialConfig(config)
{
    // qCDebug(SerialLinkLog) << this;

    (void) qRegisterMetaType<QSerialPort::SerialPortError>("QSerialPort::SerialPortError");
}

SerialWorker::~SerialWorker()
{
    disconnectFromPort();

    // qCDebug(SerialLinkLog) << this;
}

bool SerialWorker::isConnected() const
{
    return (_port && _port->isOpen());
}

void SerialWorker::setupPort()
{
    Q_ASSERT(!_port);
    _port = new QSerialPort(this);

    Q_ASSERT(!_timer);
    _timer = new QTimer(this);

    (void) connect(_port, &QSerialPort::aboutToClose, this, &SerialWorker::_onPortDisconnected);
    (void) connect(_port, &QSerialPort::readyRead, this, &SerialWorker::_onPortReadyRead);
    (void) connect(_port, &QSerialPort::errorOccurred, this, &SerialWorker::_onPortErrorOccurred);

    /* if (SerialLinkLog().isDebugEnabled()) {
        (void) connect(_port, &QSerialPort::bytesWritten, this, &SerialWorker::_onPortBytesWritten);
    } */

    (void) connect(_timer, &QTimer::timeout, this, &SerialWorker::_checkPortHealth);
    _timer->start(CONNECT_TIMEOUT_MS);
}

void SerialWorker::connectToPort()
{
    if (isConnected()) {
        qCWarning(SerialLinkLog) << "Already connected to" << _port->portName();
        return;
    }

    _port->setPortName(_serialConfig->portName());

    const QGCSerialPortInfo portInfo(*_port);
    if (portInfo.isBootloader()) {
        qCWarning(SerialLinkLog) << "Not connecting to bootloader" << _port->portName();
        emit errorOccurred(tr("Not connecting to a bootloader"));
        _onPortDisconnected();
        return;
    }

    _errorEmitted = false;
    _noDataReported = false;
    _openRetriesLeft = OPEN_RETRY_ATTEMPTS;
    _reopenAttemptsLeft = NO_DATA_REOPEN_ATTEMPTS;

    _tryOpenOrGiveUp();
}

bool SerialWorker::_openPort()
{
    qCDebug(SerialLinkLog) << "Attempting to open port" << _port->portName();

    _portTransitioning = true;
    const bool opened = _port->open(QIODevice::ReadWrite);
    _portTransitioning = false;

    if (!opened) {
        qCWarning(SerialLinkLog) << "Opening port" << _port->portName() << "failed:" << _port->errorString();
        return false;
    }

    _onPortConnected();
    return true;
}

void SerialWorker::_tryOpenOrGiveUp()
{
    if (_openPort()) {
        return;
    }

    // 先别自暴自弃：剩下的交给 _checkPortHealth 每秒重试，重试完了才认输
    if (--_openRetriesLeft <= 0) {
        _giveUpOpen();
    }
}

void SerialWorker::_giveUpOpen()
{
    qCWarning(SerialLinkLog) << "Giving up on port" << _port->portName() << ":" << _port->errorString();

    // 自动连接的链路不弹窗，靠下一轮自动连接自愈
    if (!_serialConfig->isAutoConnect() && !_errorEmitted) {
        emit errorOccurred(tr("Could not open port: %1").arg(_port->errorString()));
        _errorEmitted = true;
    }

    // 无论是否自动连接都要上报 disconnected：否则这条链路会一直留在
    // LinkManager::_rgLinks 里，_portAlreadyConnected() 会让这个端口在本次运行内
    // 再也不会被尝试连接，只能重启程序。
    _onPortDisconnected();
}

void SerialWorker::_closePort()
{
    if (!_port->isOpen()) {
        return;
    }

    qCDebug(SerialLinkLog) << "Attempting to close port:" << _port->portName();

    _portTransitioning = true;
    // 关闭前把控制线放开：DTR/RTS 是不少 USB 转串口和飞控的复位/命令模式控制线，
    // 放着不动会把对端按在那个状态里。
    _port->setDataTerminalReady(false);
    _port->setRequestToSend(false);
    _port->close();
    _portTransitioning = false;
}

void SerialWorker::disconnectFromPort()
{
    if (!isConnected()) {
        qCDebug(SerialLinkLog) << "Already disconnected from port:" << _port->portName();
        return;
    }

    // 明确断开时不再重试、不再重开
    _openRetriesLeft = 0;
    _reopenAttemptsLeft = 0;
    _closePort();
}

void SerialWorker::writeData(const QByteArray &data)
{
    if (data.isEmpty()) {
        emit errorOccurred(tr("Data to Send is Empty"));
        return;
    }

    if (!isConnected()) {
        // 重试/重开端口期间端口是短暂关着的，这时上层来的数据丢掉就行，别刷错误弹窗
        if (_openRetriesLeft > 0) {
            qCDebug(SerialLinkLog) << "Dropping data, port is not connected:" << _port->portName();
            return;
        }

        emit errorOccurred(tr("Port is not Connected"));
        return;
    }

    if (!_port->isWritable()) {
        emit errorOccurred(tr("Port is not Writable"));
        return;
    }

    qint64 totalBytesWritten = 0;
    while (totalBytesWritten < data.size()) {
        const qint64 bytesWritten = _port->write(data.constData() + totalBytesWritten, data.size() - totalBytesWritten);
        if (bytesWritten == -1) {
            emit errorOccurred(tr("Could Not Send Data - Write Failed: %1").arg(_port->errorString()));
            return;
        } else if (bytesWritten == 0) {
            emit errorOccurred(tr("Could Not Send Data - Write Returned 0 Bytes"));
            return;
        }
        totalBytesWritten += bytesWritten;
    }

    const QByteArray sent = data.first(totalBytesWritten);
    emit dataSent(sent);
}

void SerialWorker::_applyPortSettings()
{
    _port->setDataTerminalReady(true);
    _port->setBaudRate(_serialConfig->baud());
    _port->setDataBits(static_cast<QSerialPort::DataBits>(_serialConfig->dataBits()));
    _port->setFlowControl(static_cast<QSerialPort::FlowControl>(_serialConfig->flowControl()));
    _port->setStopBits(static_cast<QSerialPort::StopBits>(_serialConfig->stopBits()));
    _port->setParity(static_cast<QSerialPort::Parity>(_serialConfig->parity()));
}

void SerialWorker::_onPortConnected()
{
    qCDebug(SerialLinkLog) << "Port connected:" << _port->portName();

    _applyPortSettings();

    _errorEmitted = false;
    _bytesSinceOpen = 0;
    _lastDataTimer.start();
    emit connected();
}

void SerialWorker::_onPortDisconnected()
{
    if (_suppressDisconnect) {
        // 内部重开引起的 close：链路本身没断，不要惊动上层
        return;
    }

    qCDebug(SerialLinkLog) << "Port disconnected:" << _port->portName();
    _errorEmitted = false;
    emit disconnected();
}

void SerialWorker::_onPortReadyRead()
{
    const QByteArray data = _port->readAll();
    if (!data.isEmpty()) {
        _bytesSinceOpen += data.size();
        _lastDataTimer.restart();
        // qCDebug(SerialLinkLog) << data.size();
        emit dataReceived(data);
    }
}

void SerialWorker::_onPortBytesWritten(qint64 bytes) const
{
    qCDebug(SerialLinkLog) << _port->portName() << "Wrote" << bytes << "bytes";
}

void SerialWorker::_onPortErrorOccurred(QSerialPort::SerialPortError portError)
{
    if (_portTransitioning) {
        // open()/close() 内部同步报出来的错误由调用方处理，这里既不要弹窗也不要递归进来
        return;
    }

    const QString errorString = _port->errorString();
    qCWarning(SerialLinkLog) << "Port error:" << portError << errorString;

    switch (portError) {
    case QSerialPort::NoError:
        qCDebug(SerialLinkLog) << "About to open port" << _port->portName();
        return;
    case QSerialPort::ResourceError:
        // We get this when a usb cable is unplugged
        // Fallthrough
    case QSerialPort::PermissionError:
        // 句柄一定要放掉：旧设备对象只要还被我们打开着，Windows 就不会销毁它，
        // 而 COM3 这个名字此刻指向的已经是重新枚举出来的那个新对象。攥着旧的那份，
        // 后续要么 open 拿到"拒绝访问"，要么打开成功却永远收不到数据。
        _closePort();
        if (_serialConfig->isAutoConnect()) {
            // 自动连接不弹窗（靠下一轮自动连接自愈），但断开要上报，
            // 否则这条链路会一直挂在 _rgLinks 里让该端口不再被尝试。
            return;
        }
        break;
    default:
        break;
    }

    if (!_errorEmitted) {
        emit errorOccurred(errorString);
        _errorEmitted = true;
    }
}

void SerialWorker::_checkPortHealth()
{
    // 1) open 失败后的重试：Windows 上设备刚枚举出来、或刚被上一个持有者放掉时，
    //    open 会短暂返回 ERROR_ACCESS_DENIED（界面上就是"拒绝访问"）。
    if (!isConnected()) {
        if (_openRetriesLeft > 0) {
            qCDebug(SerialLinkLog) << "Retrying open of port" << _port->portName() << "attempts left:" << _openRetriesLeft;
            _tryOpenOrGiveUp();
        }
        return;
    }

    // 2) 连着却收不到数据：多半是 COM3 这个名字指到了重新枚举后残留的旧设备对象上
    //    （拔插、掉电、飞控重启都会触发）。关掉端口再打开就能绑回活着的那个对象，
    //    这一步就是把现场"关掉端口再打开就有数据"的手工动作自动化。
    const qint64 silence = _lastDataTimer.elapsed();
    if (silence >= SILENCE_TIMEOUT_MS) {
        if (_reopenAttemptsLeft > 0) {
            _reopenPort();
        } else if (!_noDataReported) {
            _noDataReported = true;
            qCWarning(SerialLinkLog) << "Port" << _port->portName() << "silent for" << silence
                                     << "ms (bytes since open:" << _bytesSinceOpen << "), giving up reopening";
            if (!_serialConfig->isAutoConnect()) {
                emit errorOccurred(tr("Port is open but no data was received: %1").arg(_port->portName()));
            }
        }
        return;
    }

    // 3) 拔线等导致端口从系统里消失
    if (!_portExists()) {
        _closePort();
    }
}

bool SerialWorker::_portExists() const
{
    const QString portName = _serialConfig->portName().trimmed();
    const auto availablePorts = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : availablePorts) {
        // portName 存的是 systemLocation（Windows 上是 \\.\COM3），老配置里可能只写了 COM3
        if ((info.systemLocation().trimmed() == portName) || (info.portName().trimmed() == portName)) {
            return true;
        }
    }

    return false;
}

void SerialWorker::_reopenPort()
{
    _reopenAttemptsLeft--;

    qCWarning(SerialLinkLog) << "Port" << _port->portName() << "silent for" << _lastDataTimer.elapsed()
                             << "ms, reopening, attempts left:" << _reopenAttemptsLeft;

    // 内部重开：链路本身没断，不能让 close() 走到 disconnected，
    // 否则 LinkManager 会把整条链路拆掉（界面上变成未连接）。
    _suppressDisconnect = true;
    _closePort();
    _suppressDisconnect = false;

    _openRetriesLeft = OPEN_RETRY_ATTEMPTS;
    _tryOpenOrGiveUp();
}

/*===========================================================================*/

SerialLink::SerialLink(SharedLinkConfigurationPtr &config, QObject *parent)
    : LinkInterface(config, parent)
    , _serialConfig(qobject_cast<const SerialConfiguration*>(config.get()))
    , _worker(new SerialWorker(_serialConfig))
    , _workerThread(new QThread(this))
{
    // qCDebug(SerialLinkLog) << this;

    _workerThread->setObjectName(QStringLiteral("Serial_%1").arg(_serialConfig->name()));

    (void) _worker->moveToThread(_workerThread);

    (void) connect(_workerThread, &QThread::started, _worker, &SerialWorker::setupPort);
    (void) connect(_workerThread, &QThread::finished, _worker, &QObject::deleteLater);

    (void) connect(_worker, &SerialWorker::connected, this, &SerialLink::_onConnected, Qt::QueuedConnection);
    (void) connect(_worker, &SerialWorker::disconnected, this, &SerialLink::_onDisconnected, Qt::QueuedConnection);
    (void) connect(_worker, &SerialWorker::dataReceived, this, &SerialLink::_onDataReceived, Qt::QueuedConnection);
    (void) connect(_worker, &SerialWorker::dataSent, this, &SerialLink::_onDataSent, Qt::QueuedConnection);
    (void) connect(_worker, &SerialWorker::errorOccurred, this, &SerialLink::_onErrorOccurred, Qt::QueuedConnection);

    _workerThread->start();
}

SerialLink::~SerialLink()
{
    (void) QMetaObject::invokeMethod(_worker, "disconnectFromPort", Qt::BlockingQueuedConnection);

    _workerThread->quit();
    if (!_workerThread->wait(DISCONNECT_TIMEOUT_MS)) {
        qCWarning(SerialLinkLog) << "Failed to wait for Serial Thread to close";
    }

    // qCDebug(SerialLinkLog) << this;
}

bool SerialLink::isConnected() const
{
    return _worker->isConnected();
}

bool SerialLink::_connect()
{
    return QMetaObject::invokeMethod(_worker, "connectToPort", Qt::QueuedConnection);
}

void SerialLink::disconnect()
{
    (void) QMetaObject::invokeMethod(_worker, "disconnectFromPort", Qt::QueuedConnection);
}

void SerialLink::_onConnected()
{
    emit connected();
}

void SerialLink::_onDisconnected()
{
    emit disconnected();
}

void SerialLink::_onErrorOccurred(const QString &errorString)
{
    qCWarning(SerialLinkLog) << "Communication error:" << errorString;
    emit communicationError(tr("Serial Link Error"), tr("Link %1: (Port: %2) %3").arg(_serialConfig->name(), _serialConfig->portName(), errorString));
}

void SerialLink::_onDataReceived(const QByteArray &data)
{
    emit bytesReceived(this, data);
}

void SerialLink::_onDataSent(const QByteArray &data)
{
    emit bytesSent(this, data);
}

void SerialLink::_writeBytes(const QByteArray &data)
{
    (void) QMetaObject::invokeMethod(_worker, "writeData", Qt::QueuedConnection, Q_ARG(QByteArray, data));
}
