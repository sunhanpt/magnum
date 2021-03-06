/*
    This file is part of Magnum.

    Copyright © 2010, 2011, 2012, 2013, 2014, 2015, 2016
              Vladimír Vondruš <mosra@centrum.cz>

    Permission is hereby granted, free of charge, to any person obtaining a
    copy of this software and associated documentation files (the "Software"),
    to deal in the Software without restriction, including without limitation
    the rights to use, copy, modify, merge, publish, distribute, sublicense,
    and/or sell copies of the Software, and to permit persons to whom the
    Software is furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included
    in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
    THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
    DEALINGS IN THE SOFTWARE.
*/

#include "WindowlessWindowsEglApplication.h"

#include <Corrade/Utility/Assert.h>
#include <Corrade/Utility/Debug.h>

#include "Magnum/Version.h"
#include "Magnum/Platform/Context.h"

#include "Implementation/Egl.h"

namespace Magnum { namespace Platform {

WindowlessWindowsEglContext::WindowlessWindowsEglContext(const Configuration& configuration, Context*) {
    /* Create the window */
    const WNDCLASS wc{
        0,
        DefWindowProc,
        0,
        0,
        GetModuleHandle(nullptr),
        nullptr,
        nullptr,
        HBRUSH(COLOR_BACKGROUND),
        nullptr,
        L"Magnum Windowless Application"
    };
    if(!RegisterClass(&wc)) return;
    const HWND window = CreateWindowW(wc.lpszClassName, L"Magnum Windowless Application",
        WS_OVERLAPPEDWINDOW, 0, 0, 32, 32, 0, 0, wc.hInstance, 0);

    /* Initialize */
    _display = eglGetDisplay(GetDC(window));
    if(!eglInitialize(_display, nullptr, nullptr)) {
        Error() << "Platform::WindowlessWindowsEglApplication::tryCreateContext(): cannot initialize EGL:" << Implementation::eglErrorString(eglGetError());
        return false;
    }

    const EGLenum api =
        #ifndef MAGNUM_TARGET_GLES
        EGL_OPENGL_API
        #else
        EGL_OPENGL_ES_API
        #endif
        ;
    if(!eglBindAPI(api)) {
        Error() << "Platform::WindowlessWindowsEglApplication::tryCreateContext(): cannot bind EGL API:" << Implementation::eglErrorString(eglGetError());
        return false;
    }

    /* Choose EGL config */
    static const EGLint attribs[] = {
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_DEPTH_SIZE, 1,
        #ifndef MAGNUM_TARGET_GLES
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        #elif defined(MAGNUM_TARGET_GLES3)
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        #elif defined(MAGNUM_TARGET_GLES2)
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        #else
        #error unsupported OpenGL edition
        #endif
        EGL_NONE
    };
    EGLint configCount;
    EGLConfig config;
    if(!eglChooseConfig(_display, attribs, &config, 1, &configCount)) {
        Error() << "Platform::WindowlessWindowsEglApplication::tryCreateContext(): cannot get EGL visual config:" << Implementation::eglErrorString(eglGetError());
        return false;
    }

    if(!configCount) {
        Error() << "Platform::WindowlessWindowsEglApplication::tryCreateContext(): no matching EGL visual config available";
        return false;
    }

    const EGLint attributes[] = {
        #ifdef MAGNUM_TARGET_GLES
        EGL_CONTEXT_CLIENT_VERSION,
            #ifdef MAGNUM_TARGET_GLES3
            3,
            #elif defined(MAGNUM_TARGET_GLES2)
            2,
            #else
            #error unsupported OpenGL ES version
            #endif
        #endif
        EGL_CONTEXT_FLAGS_KHR, EGLint(configuration.flags()),
        EGL_NONE
    };

    if(!(_context = eglCreateContext(_display, config, EGL_NO_CONTEXT, attributes))) {
        Error() << "Platform::WindowlessWindowsEglContext: cannot create EGL context:" << Implementation::eglErrorString(eglGetError());
        return;
    }

    if(!(_surface = eglCreateWindowSurface(_display, config, _window, nullptr)))
        Error() << "Platform::WindowlessWindowsEglContext: cannot create window surface:" << Implementation::eglErrorString(eglGetError());
}

WindowlessWindowsEglContext::WindowlessWindowsEglContext(WindowlessWindowsEglContext&& other): _window{other._window}, _display{other._display}, _surface{other._surface}, _context{other._context} {
    other._window = {};
    other._display = {};
    other._surface = {};
    other._context = {};
}

WindowlessWindowsEglContext::~WindowlessWindowsEglContext() {
    if(_context) eglDestroyContext(_display, _context);
    if(_surface) eglDestroySurface(_display, _surface);
    if(_display) eglTerminate(_display);
    if(_window) DestroyWindow(_window);
}

WindowlessWindowsEglContext& WindowlessWindowsEglContext::operator=(WindowlessWindowsEglContext&& other) {
    using std::swap;
    swap(other._window, _window);
    swap(other._display, _display);
    swap(other._surface, _surface);
    swap(other._context, _context);
    return *this;
}

bool WindowlessWindowsEglContext::makeCurrent() {
    if(eglMakeCurrent(_display, _surface, _surface, _context))
        return true;

    Error() << "Platform::WindowlessWindowsEglContext::makeCurrent(): cannot make context current:" << GetLastError();
    return false;
}

#ifndef DOXYGEN_GENERATING_OUTPUT
WindowlessWindowsEglApplication::WindowlessWindowsEglApplication(const Arguments& arguments): WindowlessWindowsEglApplication{arguments, Configuration{}} {}
#endif

WindowlessWindowsEglApplication::WindowlessWindowsEglApplication(const Arguments& arguments, const Configuration& configuration): WindowlessWindowsEglApplication{arguments, NoCreate} {
    createContext(configuration);
}

WindowlessWindowsEglApplication::WindowlessWindowsEglApplication(const Arguments& arguments, std::nullptr_t): _glContext{NoCreate}, _context{new Context{NoCreate, arguments.argc, arguments.argv}} {}

void WindowlessWindowsEglApplication::createContext() { createContext({}); }

void WindowlessWindowsEglApplication::createContext(const Configuration& configuration) {
    if(!tryCreateContext(configuration)) std::exit(1);
}

bool WindowlessWindowsEglApplication::tryCreateContext(const Configuration& configuration) {
    CORRADE_ASSERT(_context->version() == Version::None, "Platform::WindowlessWindowsEglApplication::tryCreateContext(): context already created", false);

    WindowlessWindowsEglContext glContext{configuration, _context.get()};
    if(!glContext.isCreated() || !glContext.makeCurrent() || !_context->tryCreate())
        return false;

    _glContext = std::move(glContext);
    return true;
}

WindowlessWindowsEglApplication::~WindowlessWindowsEglApplication() = default;

}}
