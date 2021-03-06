/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Quick CL module
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickclcontext.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtCore/QLoggingCategory>
#include <qpa/qplatformnativeinterface.h>

QT_BEGIN_NAMESPACE

Q_LOGGING_CATEGORY(logCL, "qt.quickcl")

/*!
    \class QQuickCLContext

    \brief QQuickCLContext encapsulates an OpenCL context.

    \note In most cases there is no need to directly interact with this class
    as QQuickCLItem takes care of creating and destroying a QQuickCLContext
    instance as necessary.

    \note This class assumes that OpenCL 1.1 and CL-GL interop are available.
 */

class QQuickCLContextPrivate
{
public:
    QQuickCLContextPrivate()
        : platform(0),
          device(0),
          context(0)
    { }

    cl_platform_id platform;
    cl_device_id device;
    cl_context context;
};

/*!
    Constructs a new instance of QQuickCLContext.

    \note No OpenCL initialization takes place before calling create().
 */
QQuickCLContext::QQuickCLContext()
    : d_ptr(new QQuickCLContextPrivate)
{
}

/*!
    Destroys the instance and releases all OpenCL resources by invoking
    destroy().
 */
QQuickCLContext::~QQuickCLContext()
{
    destroy();
    delete d_ptr;
}

/*!
    \return \c true if the OpenCL context was successfully created.
 */
bool QQuickCLContext::isValid() const
{
    Q_D(const QQuickCLContext);
    return d->context != 0;
}

/*!
    \return the OpenCL platform chosen in create().

    \note For contexts belonging to a QQuickCLItem the value is only available
    after the item is first rendered. It is always safe to call this function
    from QQuickCLRunnable's constructor, destructor and
    \l{QQuickCLRunnable::update()}{update()} function.
 */
cl_platform_id QQuickCLContext::platform() const
{
    Q_D(const QQuickCLContext);
    return d->platform;
}

/*!
    \return the OpenCL device chosen in create().

    \note For contexts belonging to a QQuickCLItem the value is only available
    after the item is first rendered. It is always safe to call this function
    from QQuickCLRunnable's constructor, destructor and
    \l{QQuickCLRunnable::update()}{update()} function.
 */
cl_device_id QQuickCLContext::device() const
{
    Q_D(const QQuickCLContext);
    return d->device;
}

/*!
    \return the OpenCL context or \c 0 if not yet created.

    \note For contexts belonging to a QQuickCLItem the value is only available
    after the item is first rendered. It is always safe to call this function
    from QQuickCLRunnable's constructor, destructor and
    \l{QQuickCLRunnable::update()}{update()} function.
 */
cl_context QQuickCLContext::context() const
{
    Q_D(const QQuickCLContext);
    return d->context;
}

/*!
    Creates a new OpenCL context.

    If a context was already created, it is destroyed first.

    An OpenGL context must be current at the time of calling this function.
    This ensures that the OpenCL platform matching the OpenGL implementation's
    vendor is selected and that CL-GL interop is enabled for the context.

    If something fails, warnings are logged with the \c qt.quickcl category.

    \return \c true if successful.
 */
bool QQuickCLContext::create()
{
    Q_D(QQuickCLContext);

    destroy();
    qCDebug(logCL, "Creating new OpenCL context");

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (!ctx) {
        qWarning("Attempted CL-GL interop without a current OpenGL context");
        return false;
    }
    QOpenGLFunctions *f = ctx->functions();

    cl_uint n;
    cl_int err = clGetPlatformIDs(0, 0, &n);
    if (err != CL_SUCCESS) {
        qWarning("Failed to get platform ID count (error %d)", err);
        if (err == -1001) {
            qWarning("Could not find OpenCL implementation. ICD missing?"
#ifdef Q_OS_LINUX
                     " Check /etc/OpenCL/vendors."
#endif
                    );
        }
        return false;
    }
    if (n == 0) {
        qWarning("No OpenCL platform found");
        return false;
    }
    QVector<cl_platform_id> platformIds;
    platformIds.resize(n);
    if (clGetPlatformIDs(n, platformIds.data(), 0) != CL_SUCCESS) {
        qWarning("Failed to get platform IDs");
        return false;
    }
    d->platform = platformIds[0];
    const char *vendor = (const char *) f->glGetString(GL_VENDOR);
    qCDebug(logCL, "GL_VENDOR: %s", vendor);
    const bool isNV = vendor && strstr(vendor, "NVIDIA");
    const bool isIntel = vendor && strstr(vendor, "Intel");
    const bool isAMD = vendor && strstr(vendor, "ATI");
    qCDebug(logCL, "Found %u OpenCL platforms:", n);
    for (cl_uint i = 0; i < n; ++i) {
        QByteArray name;
        name.resize(1024);
        clGetPlatformInfo(platformIds[i], CL_PLATFORM_NAME, name.size(), name.data(), 0);
        qCDebug(logCL, "Platform %p: %s", platformIds[i], name.constData());
        if (isNV && name.contains(QByteArrayLiteral("NVIDIA")))
            d->platform = platformIds[i];
        else if (isIntel && name.contains(QByteArrayLiteral("Intel")))
            d->platform = platformIds[i];
        else if (isAMD && name.contains(QByteArrayLiteral("AMD")))
            d->platform = platformIds[i];
    }
    qCDebug(logCL, "Using platform %p", d->platform);

#if defined (Q_OS_OSX)
    cl_context_properties contextProps[] = { CL_CONTEXT_PROPERTY_USE_CGL_SHAREGROUP_APPLE,
                                             (cl_context_properties) CGLGetShareGroup(CGLGetCurrentContext()),
                                             0 };
#elif defined(Q_OS_WIN)
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES) {
        // We don't do D3D-CL interop.
        qWarning("ANGLE is not supported");
        return false;
    }
    cl_context_properties contextProps[] = { CL_CONTEXT_PLATFORM, (cl_context_properties) d->platform,
                                             CL_GL_CONTEXT_KHR, (cl_context_properties) wglGetCurrentContext(),
                                             CL_WGL_HDC_KHR, (cl_context_properties) wglGetCurrentDC(),
                                             0 };
#elif defined(Q_OS_LINUX)
    cl_context_properties contextProps[] = { CL_CONTEXT_PLATFORM, (cl_context_properties) d->platform,
                                             CL_GL_CONTEXT_KHR, 0,
                                             0, 0,
                                             0 };
    QPlatformNativeInterface *nativeIf = qGuiApp->platformNativeInterface();
    void *dpy = nativeIf->nativeResourceForIntegration(QByteArrayLiteral("egldisplay")); // EGLDisplay
    if (dpy) {
        void *nativeContext = nativeIf->nativeResourceForContext("eglcontext", ctx);
        if (!nativeContext)
            qWarning("Failed to get the underlying EGL context from the current QOpenGLContext");
        contextProps[3] = (cl_context_properties) nativeContext;
        contextProps[4] = CL_EGL_DISPLAY_KHR;
        contextProps[5] = (cl_context_properties) dpy;
    } else {
        dpy = nativeIf->nativeResourceForIntegration(QByteArrayLiteral("display")); // Display *
        void *nativeContext = nativeIf->nativeResourceForContext("glxcontext", ctx);
        if (!nativeContext)
            qWarning("Failed to get the underlying GLX context from the current QOpenGLContext");
        contextProps[3] = (cl_context_properties) nativeContext;
        contextProps[4] = CL_GLX_DISPLAY_KHR;
        contextProps[5] = (cl_context_properties) dpy;
    }
#endif

    d->context = clCreateContextFromType(contextProps, CL_DEVICE_TYPE_GPU, 0, 0, &err);
    if (!d->context) {
        qWarning("Failed to create OpenCL context: %d", err);
        return false;
    }
    qCDebug(logCL, "Using context %p", d->context);

#if defined(Q_OS_OSX)
    err = clGetGLContextInfoAPPLE(d->context, CGLGetCurrentContext(),
                                  CL_CGL_DEVICE_FOR_CURRENT_VIRTUAL_SCREEN_APPLE,
                                  sizeof(cl_device_id), &d->device, 0);
    if (err != CL_SUCCESS) {
        qWarning("Failed to get OpenCL device for current screen: %d", err);
        destroy();
        return false;
    }
#else
    clGetGLContextInfoKHR_fn getGLContextInfo = (clGetGLContextInfoKHR_fn) clGetExtensionFunctionAddress("clGetGLContextInfoKHR");
    if (!getGLContextInfo || getGLContextInfo(contextProps, CL_CURRENT_DEVICE_FOR_GL_CONTEXT_KHR,
                                              sizeof(cl_device_id), &d->device, 0) != CL_SUCCESS) {
        err = clGetDeviceIDs(d->platform, CL_DEVICE_TYPE_GPU, 1, &d->device, 0);
        if (err != CL_SUCCESS) {
            qWarning("Failed to get OpenCL device: %d", err);
            destroy();
            return false;
        }
    }
#endif
    qCDebug(logCL, "Using device %p", d->device);

    return true;
}

/*!
    Releases all OpenCL resources.
 */
void QQuickCLContext::destroy()
{
    Q_D(QQuickCLContext);
    if (d->context) {
        qCDebug(logCL, "Releasing OpenCL context %p", d->context);
        clReleaseContext(d->context);
        d->context = 0;
    }
    d->device = 0;
    d->platform = 0;
}

/*!
    \return the name of the current platform in use.

    \note The value is valid only after create() has been called successfully.

    \note For contexts belonging to a QQuickCLItem this function can only be
    called from a QQuickCLRunnable's constructor, destructor and
    \l{QQuickCLRunnable::update()}{update()} function, or after the item has
    been rendered at least once.
 */
QByteArray QQuickCLContext::platformName() const
{
    QByteArray name(1024, '\0');
    clGetPlatformInfo(platform(), CL_PLATFORM_NAME, name.size(), name.data(), 0);
    name.resize(int(strlen(name.constData())));
    return name;
}

/*!
    \return the list of device extensions.

    \note The value is valid only after create() has been called successfully.

    \note For contexts belonging to a QQuickCLItem this function can only be
    called from a QQuickCLRunnable's constructor, destructor and
    \l{QQuickCLRunnable::update()}{update()} function, or after the item has
    been rendered at least once.
 */
QByteArray QQuickCLContext::deviceExtensions() const
{
    QByteArray ext(8192, '\0');
    clGetDeviceInfo(device(), CL_DEVICE_EXTENSIONS, ext.size(), ext.data(), 0);
    ext.resize(int(strlen(ext.constData())));
    return ext;
}

/*!
    Creates and builds an OpenCL program from the source code in \a src.

    \return the cl_program or \c 0 when failed. Errors and build logs are
    printed to the warning output.

    \note The value is valid only after create() has been called successfully.

    \note For contexts belonging to a QQuickCLItem this function can only be
    called from a QQuickCLRunnable's constructor, destructor and
    \l{QQuickCLRunnable::update()}{update()} function, or after the item has
    been rendered at least once.

    \sa buildProgramFromFile()
 */
cl_program QQuickCLContext::buildProgram(const QByteArray &src)
{
    cl_int err;
    const char *str = src.constData();
    cl_program prog = clCreateProgramWithSource(context(), 1, &str, 0, &err);
    if (!prog) {
        qWarning("Failed to create OpenCL program: %d", err);
        qWarning("Source was:\n%s", str);
        return 0;
    }
    cl_device_id dev = device();
    err = clBuildProgram(prog, 1, &dev, 0, 0, 0);
    if (err != CL_SUCCESS) {
        qWarning("Failed to build OpenCL program: %d", err);
        qWarning("Source was:\n%s", str);
        QByteArray log;
        log.resize(8192);
        clGetProgramBuildInfo(prog, dev, CL_PROGRAM_BUILD_LOG, log.size(), log.data(), 0);
        qWarning("Build log:\n%s", log.constData());
        return 0;
    }
    return prog;
}

/*!
    Creates and builds an OpenCL program from the source file \a filename.

    \note The value is valid only after create() has been called successfully.

    \note For contexts belonging to a QQuickCLItem this function can only be
    called from a QQuickCLRunnable's constructor, destructor and
    \l{QQuickCLRunnable::update()}{update()} function, or after the item has
    been rendered at least once.

    \sa buildProgram()
 */
cl_program QQuickCLContext::buildProgramFromFile(const QString &filename)
{
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning("Failed to open OpenCL program source file %s", qPrintable(filename));
        return 0;
    }
    return buildProgram(f.readAll());
}

/*!
    Returns a matching OpenCL image format for the given QImage \a format.
 */
cl_image_format QQuickCLContext::toCLImageFormat(QImage::Format format)
{
    cl_image_format fmt;

    switch (format) {
    case QImage::Format_Indexed8:
        fmt.image_channel_order = CL_A;
        fmt.image_channel_data_type = CL_UNORM_INT8;
        break;

    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
        if (QSysInfo::ByteOrder == QSysInfo::LittleEndian) {
            fmt.image_channel_order = CL_BGRA;
            fmt.image_channel_data_type = CL_UNORM_INT8;
        } else {
            fmt.image_channel_order = CL_ARGB;
            fmt.image_channel_data_type = CL_UNORM_INT8;
        }
        break;

    case QImage::Format_RGB16:
        fmt.image_channel_order = CL_RGB;
        fmt.image_channel_data_type = CL_UNORM_SHORT_565;
        break;

    case QImage::Format_RGB555:
        fmt.image_channel_order = CL_RGB;
        fmt.image_channel_data_type = CL_UNORM_SHORT_555;
        break;

    case QImage::Format_RGB888:
        fmt.image_channel_order = CL_RGB;
        fmt.image_channel_data_type = CL_UNORM_INT8;
        break;

    case QImage::Format_RGBX8888:
        fmt.image_channel_order = CL_RGBx;
        fmt.image_channel_data_type = CL_UNORM_INT8;
        break;

    case QImage::Format_RGBA8888:
    case QImage:: Format_RGBA8888_Premultiplied:
        fmt.image_channel_order = CL_RGBA;
        fmt.image_channel_data_type = CL_UNORM_INT8;
        break;

    default:
        qWarning("toCLImageFormat: Unrecognized QImage format %d", format);
        fmt.image_channel_order = 0;
        fmt.image_channel_data_type = 0;
        break;
    }

    return fmt;
}

QT_END_NAMESPACE
