# Copyright (c) 2017 Intel Corporation
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

find_path( OPENGL_INCLUDE GL/gl.h EGL/egl.h EGL/eglext.h gbm.h PATHS /usr/local/include /usr/include)
find_library( OPENGL_LIBRARY libGL.so libEGL.so libgbm.so PATHS /usr/local/lib64 /usr/lib64)

if (ENABLE_OPENGL)
    if ( NOT OPENGL_INCLUDE MATCHES NOTFOUND )
        if ( NOT OPENGL_LIBRARY MATCHES NOTFOUND )
            set( OPENGL_FOUND TRUE )
            add_definitions( -DUSE_OPENGL )
            get_filename_component( OPENGL_LIBRARY_PATH ${OPENGL_LIBRARY} PATH )
            list( APPEND OPENGL_LIBS OpenGL )
        endif()
    endif()
endif()

if ( NOT DEFINED OPENGL_FOUND )
  message( STATUS "OpenGL was not found (optional) or not required by the user. The following will not be built: render + encode pipeline.")
else ()
  message( STATUS "OpenGL was found here: ${OPENGL_LIBRARY_PATH} and ${OPENGL_INCLUDE}" )
endif()
