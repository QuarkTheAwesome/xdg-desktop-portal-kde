/*
 * Copyright © 2018 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Jan Grulich <jgrulich@redhat.com>
 */

#include "waylandintegration.h"
#include "waylandintegration_p.h"


#include <QDBusArgument>
#include <QDBusMetaType>

#include <QEventLoop>
#include <QLoggingCategory>
#include <QThread>
#include <QTimer>

#include <QImage>

#include <KLocalizedString>

// KWayland
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/event_queue.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmawindowmanagement.h>

// system
#include <fcntl.h>
#include <unistd.h>

#if HAVE_PIPEWIRE_SUPPORT
#include "screencaststream.h"

#include <KWayland/Client/fakeinput.h>
#include <KWayland/Client/output.h>
#include <KWayland/Client/remote_access.h>
#endif

Q_LOGGING_CATEGORY(XdgDesktopPortalKdeWaylandIntegration, "xdp-kde-wayland-integration")

Q_GLOBAL_STATIC(WaylandIntegration::WaylandIntegrationPrivate, globalWaylandIntegration)

void WaylandIntegration::init()
{
#if HAVE_PIPEWIRE_SUPPORT
    globalWaylandIntegration->initDrm();
#endif
    globalWaylandIntegration->initWayland();
}

#if HAVE_PIPEWIRE_SUPPORT
void WaylandIntegration::authenticate()
{
    globalWaylandIntegration->authenticate();
}

bool WaylandIntegration::isEGLInitialized()
{
    return globalWaylandIntegration->isEGLInitialized();
}

bool WaylandIntegration::isStreamingEnabled()
{
    return globalWaylandIntegration->isStreamingEnabled();
}

void WaylandIntegration::startStreamingInput()
{
    globalWaylandIntegration->startStreamingInput();
}

bool WaylandIntegration::startStreaming(quint32 outputName)
{
    return globalWaylandIntegration->startStreaming(outputName);
}

void WaylandIntegration::stopStreaming()
{
    globalWaylandIntegration->stopStreaming();
}

void WaylandIntegration::requestPointerButtonPress(quint32 linuxButton)
{
    globalWaylandIntegration->requestPointerButtonPress(linuxButton);
}

void WaylandIntegration::requestPointerButtonRelease(quint32 linuxButton)
{
    globalWaylandIntegration->requestPointerButtonRelease(linuxButton);
}

void WaylandIntegration::requestPointerMotion(const QSizeF &delta)
{
    globalWaylandIntegration->requestPointerMotion(delta);
}

void WaylandIntegration::requestPointerMotionAbsolute(const QPointF &pos)
{
    globalWaylandIntegration->requestPointerMotionAbsolute(pos);
}

void WaylandIntegration::requestPointerAxisDiscrete(Qt::Orientation axis, qreal delta)
{
    globalWaylandIntegration->requestPointerAxisDiscrete(axis, delta);
}

void WaylandIntegration::requestKeyboardKeycode(int keycode, bool state)
{
    globalWaylandIntegration->requestKeyboardKeycode(keycode, state);
}

WaylandIntegration::EGLStruct WaylandIntegration::egl()
{
    return globalWaylandIntegration->egl();
}

QMap<quint32, WaylandIntegration::WaylandOutput> WaylandIntegration::screens()
{
    return globalWaylandIntegration->screens();
}

QVariant WaylandIntegration::streams()
{
    return globalWaylandIntegration->streams();
}

const char * WaylandIntegration::formatGLError(GLenum err)
{
    switch(err) {
    case GL_NO_ERROR:
        return "GL_NO_ERROR";
    case GL_INVALID_ENUM:
        return "GL_INVALID_ENUM";
    case GL_INVALID_VALUE:
        return "GL_INVALID_VALUE";
    case GL_INVALID_OPERATION:
        return "GL_INVALID_OPERATION";
    case GL_STACK_OVERFLOW:
        return "GL_STACK_OVERFLOW";
    case GL_STACK_UNDERFLOW:
        return "GL_STACK_UNDERFLOW";
    case GL_OUT_OF_MEMORY:
        return "GL_OUT_OF_MEMORY";
    default:
        return (QLatin1String("0x") + QString::number(err, 16)).toLocal8Bit().constData();
    }
}

// Thank you kscreen
void WaylandIntegration::WaylandOutput::setOutputType(const QString &type)
{
    const auto embedded = { QLatin1String("LVDS"),
                                   QLatin1String("IDP"),
                                   QLatin1String("EDP"),
                                   QLatin1String("LCD") };

    for (const QLatin1String &pre : embedded) {
        if (type.toUpper().startsWith(pre)) {
            m_outputType = OutputType::Laptop;
            return;
        }
    }

    if (type.contains(QLatin1String("VGA")) || type.contains(QLatin1String("DVI")) || type.contains(QLatin1String("HDMI")) || type.contains(QLatin1String("Panel")) ||
        type.contains(QLatin1String("DisplayPort")) || type.startsWith(QLatin1String("DP")) || type.contains(QLatin1String("unknown"))) {
        m_outputType = OutputType::Monitor;
    } else if (type.contains(QLatin1String("TV"))) {
        m_outputType = OutputType::Television;
    } else {
        m_outputType = OutputType::Monitor;
    }
}

const QDBusArgument &operator >> (const QDBusArgument &arg, WaylandIntegration::WaylandIntegrationPrivate::Stream &stream)
{
    arg.beginStructure();
    arg >> stream.nodeId;

    arg.beginMap();
    while (!arg.atEnd()) {
        QString key;
        QVariant map;
        arg.beginMapEntry();
        arg >> key >> map;
        arg.endMapEntry();
        stream.map.insert(key, map);
    }
    arg.endMap();
    arg.endStructure();

    return arg;
}

const QDBusArgument &operator << (QDBusArgument &arg, const WaylandIntegration::WaylandIntegrationPrivate::Stream &stream)
{
    arg.beginStructure();
    arg << stream.nodeId;
    arg << stream.map;
    arg.endStructure();

    return arg;
}

Q_DECLARE_METATYPE(WaylandIntegration::WaylandIntegrationPrivate::Stream)
Q_DECLARE_METATYPE(WaylandIntegration::WaylandIntegrationPrivate::Streams)
#endif

KWayland::Client::PlasmaWindowManagement * WaylandIntegration::plasmaWindowManagement()
{
    return globalWaylandIntegration->plasmaWindowManagement();
}

WaylandIntegration::WaylandIntegration * WaylandIntegration::waylandIntegration()
{
    return globalWaylandIntegration;
}

WaylandIntegration::WaylandIntegrationPrivate::WaylandIntegrationPrivate()
    : WaylandIntegration()
    , m_registryInitialized(false)
    , m_connection(nullptr)
    , m_queue(nullptr)
    , m_registry(nullptr)
#if HAVE_PIPEWIRE_SUPPORT
    , m_fakeInput(nullptr)
    , m_remoteAccessManager(nullptr)
#endif
{
#if HAVE_PIPEWIRE_SUPPORT
    qDBusRegisterMetaType<WaylandIntegrationPrivate::Stream>();
    qDBusRegisterMetaType<WaylandIntegrationPrivate::Streams>();
#endif
}

WaylandIntegration::WaylandIntegrationPrivate::~WaylandIntegrationPrivate()
{
#if HAVE_PIPEWIRE_SUPPORT
    if (m_remoteAccessManager) {
        m_remoteAccessManager->destroy();
    }

    if (m_gbmDevice) {
        gbm_device_destroy(m_gbmDevice);
    }
#endif
}

#if HAVE_PIPEWIRE_SUPPORT
bool WaylandIntegration::WaylandIntegrationPrivate::isEGLInitialized() const
{
    return m_eglInitialized;
}

bool WaylandIntegration::WaylandIntegrationPrivate::isStreamingEnabled() const
{
    return m_streamingEnabled;
}

void WaylandIntegration::WaylandIntegrationPrivate::bindOutput(int outputName, int outputVersion)
{
    KWayland::Client::Output *output = new KWayland::Client::Output(this);
    output->setup(m_registry->bindOutput(outputName, outputVersion));
    m_bindOutputs << output;
}

void WaylandIntegration::WaylandIntegrationPrivate::startStreamingInput()
{
    m_streamInput = true;
}

bool WaylandIntegration::WaylandIntegrationPrivate::startStreaming(quint32 outputName)
{
    WaylandOutput output = m_outputMap.value(outputName);
    m_streamedScreenPosition = output.globalPosition();

    m_stream = new ScreenCastStream(output.resolution());
    m_stream->init();

    connect(m_stream, &ScreenCastStream::startStreaming, this, [this, output] {
        m_streamingEnabled = true;
        startStreamingInput();
        bindOutput(output.waylandOutputName(), output.waylandOutputVersion());
    });

    connect(m_stream, &ScreenCastStream::stopStreaming, this, &WaylandIntegrationPrivate::stopStreaming);

    bool streamReady = false;
    QEventLoop loop;
    connect(m_stream, &ScreenCastStream::streamReady, this, [&loop, &streamReady] {
        loop.quit();
        streamReady = true;
    });

    // HACK wait for stream to be ready
    QTimer::singleShot(3000, &loop, &QEventLoop::quit);
    loop.exec();

    disconnect(m_stream, &ScreenCastStream::streamReady, this, nullptr);

    if (!streamReady) {
        if (m_stream) {
            delete m_stream;
            m_stream = nullptr;
        }
        return false;
    }

    // TODO support multiple outputs

    if (m_registry->hasInterface(KWayland::Client::Registry::Interface::RemoteAccessManager)) {
        KWayland::Client::Registry::AnnouncedInterface interface = m_registry->interface(KWayland::Client::Registry::Interface::RemoteAccessManager);
        if (!interface.name && !interface.version) {
            qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "Failed to start streaming: remote access manager interface is not initialized yet";
            return false;
        }
        m_remoteAccessManager = m_registry->createRemoteAccessManager(interface.name, interface.version);
        connect(m_remoteAccessManager, &KWayland::Client::RemoteAccessManager::bufferReady, this, [this] (const void *output, const KWayland::Client::RemoteBuffer * rbuf) {
            Q_UNUSED(output);
            connect(rbuf, &KWayland::Client::RemoteBuffer::parametersObtained, this, [this, rbuf] {
                processBuffer(rbuf);
            });
        });
        m_output = output.waylandOutputName();
        return true;
    }

    if (m_stream) {
        delete m_stream;
        m_stream = nullptr;
    }

    qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "Failed to start streaming: no remote access manager interface";
    return false;
}

void WaylandIntegration::WaylandIntegrationPrivate::stopStreaming()
{
    m_streamInput = false;
    if (m_streamingEnabled) {
        m_streamingEnabled = false;

        // First unbound outputs and destroy remote access manager so we no longer receive buffers
        if (m_remoteAccessManager) {
            m_remoteAccessManager->release();
            m_remoteAccessManager->destroy();
        }
        qDeleteAll(m_bindOutputs);
        m_bindOutputs.clear();

        if (m_stream) {
            delete m_stream;
            m_stream = nullptr;
        }
    }
}

void WaylandIntegration::WaylandIntegrationPrivate::requestPointerButtonPress(quint32 linuxButton)
{
    if (m_streamInput && m_fakeInput) {
        m_fakeInput->requestPointerButtonPress(linuxButton);
    }
}

void WaylandIntegration::WaylandIntegrationPrivate::requestPointerButtonRelease(quint32 linuxButton)
{
    if (m_streamInput && m_fakeInput) {
        m_fakeInput->requestPointerButtonRelease(linuxButton);
    }
}

void WaylandIntegration::WaylandIntegrationPrivate::requestPointerMotion(const QSizeF &delta)
{
    if (m_streamInput && m_fakeInput) {
        m_fakeInput->requestPointerMove(delta);
    }
}

void WaylandIntegration::WaylandIntegrationPrivate::requestPointerMotionAbsolute(const QPointF &pos)
{
    if (m_streamInput && m_fakeInput) {
        m_fakeInput->requestPointerMoveAbsolute(pos + m_streamedScreenPosition);
    }
}

void WaylandIntegration::WaylandIntegrationPrivate::requestPointerAxisDiscrete(Qt::Orientation axis, qreal delta)
{
    if (m_streamInput && m_fakeInput) {
        m_fakeInput->requestPointerAxis(axis, delta);
    }
}

void WaylandIntegration::WaylandIntegrationPrivate::requestKeyboardKeycode(int keycode, bool state)
{
    if (m_streamInput && m_fakeInput) {
        if (state) {
            m_fakeInput->requestKeyboardKeyPress(keycode);
        } else {
            m_fakeInput->requestKeyboardKeyRelease(keycode);
        }
    }
}

WaylandIntegration::EGLStruct WaylandIntegration::WaylandIntegrationPrivate::egl()
{
    return m_egl;
}

QMap<quint32, WaylandIntegration::WaylandOutput> WaylandIntegration::WaylandIntegrationPrivate::screens()
{
    return m_outputMap;
}

QVariant WaylandIntegration::WaylandIntegrationPrivate::streams()
{
    Stream stream;
    stream.nodeId = m_stream->nodeId();
    stream.map = QVariantMap({{QLatin1String("size"), m_outputMap.value(m_output).resolution()}});
    return QVariant::fromValue<WaylandIntegrationPrivate::Streams>({stream});
}

void WaylandIntegration::WaylandIntegrationPrivate::initDrm()
{
    m_drmFd = open("/dev/dri/renderD128", O_RDWR);

    if (m_drmFd == -1) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "Cannot open render node: " << strerror(errno);
        return;
    }

    m_gbmDevice = gbm_create_device(m_drmFd);

    if (!m_gbmDevice) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "Cannot create GBM device: " << strerror(errno);
    }

    initEGL();
}

void WaylandIntegration::WaylandIntegrationPrivate::initEGL()
{
   // Get the list of client extensions
    const char* clientExtensionsCString = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    const QByteArray clientExtensionsString = QByteArray::fromRawData(clientExtensionsCString, qstrlen(clientExtensionsCString));
    if (clientExtensionsString.isEmpty()) {
        // If eglQueryString() returned NULL, the implementation doesn't support
        // EGL_EXT_client_extensions. Expect an EGL_BAD_DISPLAY error.
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "No client extensions defined! " << formatGLError(eglGetError());
        return;
    }

    m_egl.extensions = clientExtensionsString.split(' ');

    // Use eglGetPlatformDisplayEXT() to get the display pointer
    // if the implementation supports it.
    if (!m_egl.extensions.contains(QByteArrayLiteral("EGL_EXT_platform_base")) ||
            !m_egl.extensions.contains(QByteArrayLiteral("EGL_MESA_platform_gbm"))) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "One of required EGL extensions is missing";
        return;
    }

    m_egl.display = eglGetPlatformDisplayEXT(EGL_PLATFORM_GBM_MESA, m_gbmDevice, nullptr);

    if (m_egl.display == EGL_NO_DISPLAY) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "Error during obtaining EGL display: " << formatGLError(eglGetError());
        return;
    }

    EGLint major, minor;
    if (eglInitialize(m_egl.display, &major, &minor) == EGL_FALSE) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "Error during eglInitialize: " << formatGLError(eglGetError());
        return;
    }

    if (eglBindAPI(EGL_OPENGL_API) == EGL_FALSE) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "bind OpenGL API failed";
        return;
    }

    m_egl.context = eglCreateContext(m_egl.display, nullptr, EGL_NO_CONTEXT, nullptr);

    if (m_egl.context == EGL_NO_CONTEXT) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "Couldn't create EGL context: " << formatGLError(eglGetError());
        return;
    }

    qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "Egl initialization succeeded";
    qCDebug(XdgDesktopPortalKdeWaylandIntegration) << QStringLiteral("EGL version: %1.%2").arg(major).arg(minor);

    m_eglInitialized = true;
}

void WaylandIntegration::WaylandIntegrationPrivate::authenticate()
{
    if (!m_waylandAuthenticationRequested) {
        m_fakeInput->authenticate(i18n("xdg-desktop-portals-kde"), i18n("Remote desktop"));
        m_waylandAuthenticationRequested = true;
    }
}

#endif

KWayland::Client::PlasmaWindowManagement * WaylandIntegration::WaylandIntegrationPrivate::plasmaWindowManagement()
{
    return m_windowManagement;
}

void WaylandIntegration::WaylandIntegrationPrivate::initWayland()
{
    m_thread = new QThread(this);
    m_connection = new KWayland::Client::ConnectionThread;

    connect(m_connection, &KWayland::Client::ConnectionThread::connected, this, &WaylandIntegrationPrivate::setupRegistry, Qt::QueuedConnection);
    connect(m_connection, &KWayland::Client::ConnectionThread::connectionDied, this, [this] {
        if (m_queue) {
            delete m_queue;
            m_queue = nullptr;
        }

        m_connection->deleteLater();
        m_connection = nullptr;

        if (m_thread) {
            m_thread->quit();
            if (!m_thread->wait(3000)) {
                m_thread->terminate();
                m_thread->wait();
            }
            delete m_thread;
            m_thread = nullptr;
        }
    });
    connect(m_connection, &KWayland::Client::ConnectionThread::failed, this, [this] {
        m_thread->quit();
        m_thread->wait();
    });

    m_thread->start();
    m_connection->moveToThread(m_thread);
    m_connection->initConnection();
}

#if HAVE_PIPEWIRE_SUPPORT
void WaylandIntegration::WaylandIntegrationPrivate::addOutput(quint32 name, quint32 version)
{
    KWayland::Client::Output *output = new KWayland::Client::Output(this);
    output->setup(m_registry->bindOutput(name, version));

    connect(output, &KWayland::Client::Output::changed, this, [this, name, version, output] () {
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "Adding output:";
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    manufacturer: " << output->manufacturer();
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    model: " << output->model();
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    resolution: " << output->pixelSize();

        WaylandOutput portalOutput;
        portalOutput.setManufacturer(output->manufacturer());
        portalOutput.setModel(output->model());
        portalOutput.setOutputType(output->model());
        portalOutput.setGlobalPosition(output->globalPosition());
        portalOutput.setResolution(output->pixelSize());
        portalOutput.setWaylandOutputName(name);
        portalOutput.setWaylandOutputVersion(version);

        m_outputMap.insert(name, portalOutput);

        delete output;
    });
}

void WaylandIntegration::WaylandIntegrationPrivate::removeOutput(quint32 name)
{
    WaylandOutput output = m_outputMap.take(name);
    qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "Removing output:";
    qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    manufacturer: " << output.manufacturer();
    qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "    model: " << output.model();
}

void WaylandIntegration::WaylandIntegrationPrivate::processBuffer(const KWayland::Client::RemoteBuffer* rbuf)
{
    QScopedPointer<const KWayland::Client::RemoteBuffer> guard(rbuf);

    auto gbmHandle = rbuf->fd();
    auto width = rbuf->width();
    auto height = rbuf->height();
    auto stride = rbuf->stride();
    auto format = rbuf->format();

    qCDebug(XdgDesktopPortalKdeWaylandIntegration) << QStringLiteral("Incoming GBM fd %1, %2x%3, stride %4, fourcc 0x%5").arg(gbmHandle).arg(width).arg(height).arg(stride).arg(QString::number(format, 16));

    if (!m_streamingEnabled) {
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "Streaming is disabled";
        close(gbmHandle);
        return;
    }

    if (m_lastFrameTime.isValid() &&
        m_lastFrameTime.msecsTo(QDateTime::currentDateTime()) < (1000 / m_stream->framerate())) {
        close(gbmHandle);
        return;
    }

    if (!gbm_device_is_format_supported(m_gbmDevice, format, GBM_BO_USE_SCANOUT)) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "Failed to process buffer: GBM format is not supported by device!";
        close(gbmHandle);
        return;
    }

    // import GBM buffer that was passed from KWin
    gbm_import_fd_data importInfo = {gbmHandle, width, height, stride, format};
    gbm_bo *imported = gbm_bo_import(m_gbmDevice, GBM_BO_IMPORT_FD, &importInfo, GBM_BO_USE_SCANOUT);
    if (!imported) {
        qCWarning(XdgDesktopPortalKdeWaylandIntegration) << "Failed to process buffer: Cannot import passed GBM fd - " << strerror(errno);
        close(gbmHandle);
        return;
    }

    if (m_stream->recordFrame(imported, width, height, stride)) {
        m_lastFrameTime = QDateTime::currentDateTime();
    }

    gbm_bo_destroy(imported);
    close(gbmHandle);
}
#endif

void WaylandIntegration::WaylandIntegrationPrivate::setupRegistry()
{
    m_queue = new KWayland::Client::EventQueue(this);
    m_queue->setup(m_connection);

    m_registry = new KWayland::Client::Registry(this);

#if HAVE_PIPEWIRE_SUPPORT
    connect(m_registry, &KWayland::Client::Registry::fakeInputAnnounced, this, [this] (quint32 name, quint32 version) {
        m_fakeInput = m_registry->createFakeInput(name, version, this);
    });
    connect(m_registry, &KWayland::Client::Registry::outputAnnounced, this, &WaylandIntegrationPrivate::addOutput);
    connect(m_registry, &KWayland::Client::Registry::outputRemoved, this, &WaylandIntegrationPrivate::removeOutput);
#endif

    connect(m_registry, &KWayland::Client::Registry::plasmaWindowManagementAnnounced, this, [this] (quint32 name, quint32 version) {
        m_windowManagement = m_registry->createPlasmaWindowManagement(name, version, this);
        Q_EMIT waylandIntegration()->plasmaWindowManagementInitialized();
    });

    connect(m_registry, &KWayland::Client::Registry::interfacesAnnounced, this, [this] {
        m_registryInitialized = true;
        qCDebug(XdgDesktopPortalKdeWaylandIntegration) << "Registry initialized";
    });

    m_registry->create(m_connection);
    m_registry->setEventQueue(m_queue);
    m_registry->setup();
}
