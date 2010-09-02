
set(INCROOT ${CMAKE_SOURCE_DIR}/include/SFML/Window)
set(SRCROOT ${CMAKE_SOURCE_DIR}/src/SFML/Window)

# all source files
set(SRC
    ${SRCROOT}/Context.cpp
    ${INCROOT}/Context.hpp
    ${SRCROOT}/GlContext.cpp
    ${SRCROOT}/GlContext.hpp
    ${INCROOT}/ContextSettings.hpp
    ${INCROOT}/Event.hpp
    ${SRCROOT}/Input.cpp
    ${INCROOT}/Input.hpp
    ${SRCROOT}/Joystick.hpp
    ${SRCROOT}/VideoMode.cpp
    ${INCROOT}/VideoMode.hpp
    ${SRCROOT}/VideoModeImpl.hpp
    ${SRCROOT}/Window.cpp
    ${INCROOT}/Window.hpp
    ${INCROOT}/WindowHandle.hpp
    ${SRCROOT}/WindowImpl.cpp
    ${SRCROOT}/WindowImpl.hpp
    ${INCROOT}/WindowStyle.hpp
)

# add platform specific sources
if(WINDOWS)
    set(SRC
        ${SRC}
        ${SRCROOT}/Win32/WglContext.cpp
        ${SRCROOT}/Win32/WglContext.hpp
        ${SRCROOT}/Win32/Joystick.cpp
        ${SRCROOT}/Win32/Joystick.hpp
        ${SRCROOT}/Win32/VideoModeImpl.cpp
        ${SRCROOT}/Win32/WindowImplWin32.cpp
        ${SRCROOT}/Win32/WindowImplWin32.hpp
    )
elseif(LINUX)
    set(SRC
        ${SRC}
        ${SRCROOT}/Linux/GlxContext.cpp
        ${SRCROOT}/Linux/GlxContext.hpp
        ${SRCROOT}/Linux/Joystick.cpp
        ${SRCROOT}/Linux/Joystick.hpp
        ${SRCROOT}/Linux/VideoModeImpl.cpp
        ${SRCROOT}/Linux/WindowImplX11.cpp
        ${SRCROOT}/Linux/WindowImplX11.hpp
    )
else()
    set(SRC
        ${SRC}
        ${SRCROOT}/Cocoa/AppController.h
        ${SRCROOT}/Cocoa/AppController.mm
        ${SRCROOT}/Cocoa/GLKit.h
        ${SRCROOT}/Cocoa/GLKit.mm
        ${SRCROOT}/Cocoa/Joystick.cpp
        ${SRCROOT}/Cocoa/Joystick.hpp
        ${SRCROOT}/Cocoa/VideoModeImpl.cpp
        ${SRCROOT}/Cocoa/WindowImplCocoa.cpp
        ${SRCROOT}/Cocoa/WindowImplCocoa.mm
    )
endif()

# find external libraries
find_package(OpenGL REQUIRED)
if(LINUX)
    find_package(X11 REQUIRED)
endif()

# build the list of external libraries to link
set(WINDOW_EXT_LIBS ${OPENGL_gl_LIBRARY})
if(WINDOWS)
    set(WINDOW_EXT_LIBS ${WINDOW_EXT_LIBS} winmm gdi32)
else(LINUX)
    set(WINDOW_EXT_LIBS ${WINDOW_EXT_LIBS} ${X11_X11_LIB} ${X11_Xrandr_LIB})
endif()

# define the sfml-window target
sfml_add_library(sfml-window
                 SOURCES ${SRC}
                 DEPENDS sfml-system
                 EXTERNAL_LIBS ${WINDOW_EXT_LIBS})