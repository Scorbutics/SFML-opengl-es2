////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2023 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include <SFML/Graphics/Shader.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/Transform.hpp>
#include <SFML/Graphics/Color.hpp>
#include <SFML/Graphics/GLCheck.hpp>
#include <SFML/Window/Context.hpp>
#include <SFML/System/InputStream.hpp>
#include <SFML/System/Mutex.hpp>
#include <SFML/System/Lock.hpp>
#include <SFML/System/Err.hpp>
#include <fstream>
#include <vector>


#if defined(SFML_SYSTEM_MACOS) || defined(SFML_SYSTEM_IOS)

    #define castToGlHandle(x) reinterpret_cast<GLEXT_GLhandle>(static_cast<ptrdiff_t>(x))
    #define castFromGlHandle(x) static_cast<unsigned int>(reinterpret_cast<ptrdiff_t>(x))

#else

    #define castToGlHandle(x) (x)
    #define castFromGlHandle(x) (x)

#endif

namespace
{
    sf::Mutex maxTextureUnitsMutex;
    sf::Mutex isAvailableMutex;

    GLint checkMaxTextureUnits()
    {
        GLint maxUnits = 0;
        glCheck(glGetIntegerv(GLEXT_GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxUnits));

        return maxUnits;
    }

    // Retrieve the maximum number of texture units available
    std::size_t getMaxTextureUnits()
    {
        // TODO: Remove this lock when it becomes unnecessary in C++11
        sf::Lock lock(maxTextureUnitsMutex);

        static GLint maxUnits = checkMaxTextureUnits();

        return static_cast<std::size_t>(maxUnits);
    }

    // Read the contents of a file into an array of char
    bool getFileContents(const std::string& filename, std::vector<char>& buffer)
    {
        std::ifstream file(filename.c_str(), std::ios_base::binary);
        if (file)
        {
            file.seekg(0, std::ios_base::end);
            std::ifstream::pos_type size = file.tellg();
            if (size > 0)
            {
                file.seekg(0, std::ios_base::beg);
                buffer.resize(static_cast<std::size_t>(size));
                file.read(&buffer[0], static_cast<std::streamsize>(size));
            }
            buffer.push_back('\0');
            return true;
        }
        else
        {
            return false;
        }
    }

    // Read the contents of a stream into an array of char
    bool getStreamContents(sf::InputStream& stream, std::vector<char>& buffer)
    {
        bool success = true;
        sf::Int64 size = stream.getSize();
        if (size > 0)
        {
            buffer.resize(static_cast<std::size_t>(size));
            stream.seek(0);
            sf::Int64 read = stream.read(&buffer[0], size);
            success = (read == size);
        }
        buffer.push_back('\0');
        return success;
    }

    // Transforms an array of 2D vectors into a contiguous array of scalars
    template <typename T>
    std::vector<T> flatten(const sf::Vector2<T>* vectorArray, std::size_t length)
    {
        const std::size_t vectorSize = 2;

        std::vector<T> contiguous(vectorSize * length);
        for (std::size_t i = 0; i < length; ++i)
        {
            contiguous[vectorSize * i]     = vectorArray[i].x;
            contiguous[vectorSize * i + 1] = vectorArray[i].y;
        }

        return contiguous;
    }

    // Transforms an array of 3D vectors into a contiguous array of scalars
    template <typename T>
    std::vector<T> flatten(const sf::Vector3<T>* vectorArray, std::size_t length)
    {
        const std::size_t vectorSize = 3;

        std::vector<T> contiguous(vectorSize * length);
        for (std::size_t i = 0; i < length; ++i)
        {
            contiguous[vectorSize * i]     = vectorArray[i].x;
            contiguous[vectorSize * i + 1] = vectorArray[i].y;
            contiguous[vectorSize * i + 2] = vectorArray[i].z;
        }

        return contiguous;
    }

    // Transforms an array of 4D vectors into a contiguous array of scalars
    template <typename T>
    std::vector<T> flatten(const sf::priv::Vector4<T>* vectorArray, std::size_t length)
    {
        const std::size_t vectorSize = 4;

        std::vector<T> contiguous(vectorSize * length);
        for (std::size_t i = 0; i < length; ++i)
        {
            contiguous[vectorSize * i]     = vectorArray[i].x;
            contiguous[vectorSize * i + 1] = vectorArray[i].y;
            contiguous[vectorSize * i + 2] = vectorArray[i].z;
            contiguous[vectorSize * i + 3] = vectorArray[i].w;
        }

        return contiguous;
    }
}


namespace sf
{
////////////////////////////////////////////////////////////
Shader::CurrentTextureType Shader::CurrentTexture;


////////////////////////////////////////////////////////////
struct Shader::UniformBinder : private NonCopyable
{
    ////////////////////////////////////////////////////////////
    /// \brief Constructor: set up state before uniform is set
    ///
    ////////////////////////////////////////////////////////////
    UniformBinder(Shader& shader, const std::string& name) :
    savedProgram(0),
    currentProgram(static_cast<GLEXT_GLhandle>(castToGlHandle(shader.m_shaderProgram))),
    location(-1)
    {
        if (currentProgram)
        {

// Enable program object
#ifndef SFML_OPENGL_ES
            glCheck(savedProgram = GLEXT_glGetHandle(GLEXT_GL_PROGRAM_OBJECT));
#else
            glCheck(glGetIntegerv(GL_CURRENT_PROGRAM, reinterpret_cast<GLint*>(&savedProgram)));
#endif
            if (currentProgram != savedProgram)
                glCheck(GLEXT_glUseProgramObject(currentProgram));
            // Store uniform location for further use outside constructor
            location = shader.getUniformLocation(name);
        }
    }

    ////////////////////////////////////////////////////////////
    /// \brief Destructor: restore state after uniform is set
    ///
    ////////////////////////////////////////////////////////////
    ~UniformBinder()
    {
        // Disable program object
        if (currentProgram && (currentProgram != savedProgram))
            glCheck(GLEXT_glUseProgramObject(savedProgram));
    }

    TransientContextLock lock;           //!< Lock to keep context active while uniform is bound
    GLEXT_GLhandle       savedProgram;   //!< Handle to the previously active program object
    GLEXT_GLhandle       currentProgram; //!< Handle to the program object of the modified sf::Shader instance
    GLint                location;       //!< Uniform location, used by the surrounding sf::Shader code
};


////////////////////////////////////////////////////////////
Shader::Shader() :
m_shaderProgram (0),
m_currentTexture(-1),
m_textures      (),
m_uniforms      ()
{
}


////////////////////////////////////////////////////////////
Shader::~Shader()
{
    TransientContextLock lock;

    // Destroy effect program
    if (m_shaderProgram)
        glCheck(GLEXT_glDeleteProgram(castToGlHandle(m_shaderProgram)));
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const std::string& filename, Type type)
{
    // Read the file
    std::vector<char> shader;
    if (!getFileContents(filename, shader))
    {
        err() << "Failed to open shader file \"" << filename << "\"" << std::endl;
        return false;
    }

    // Compile the shader program
    if (type == Vertex)
        return compile(&shader[0], NULL, NULL);
    else if (type == Geometry)
        return compile(NULL, &shader[0], NULL);
    else
        return compile(NULL, NULL, &shader[0]);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const std::string& vertexShaderFilename, const std::string& fragmentShaderFilename)
{
    // Read the vertex shader file
    std::vector<char> vertexShader;
    if (!getFileContents(vertexShaderFilename, vertexShader))
    {
        err() << "Failed to open vertex shader file \"" << vertexShaderFilename << "\"" << std::endl;
        return false;
    }

    // Read the fragment shader file
    std::vector<char> fragmentShader;
    if (!getFileContents(fragmentShaderFilename, fragmentShader))
    {
        err() << "Failed to open fragment shader file \"" << fragmentShaderFilename << "\"" << std::endl;
        return false;
    }

    // Compile the shader program
    return compile(&vertexShader[0], NULL, &fragmentShader[0]);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromFile(const std::string& vertexShaderFilename, const std::string& geometryShaderFilename, const std::string& fragmentShaderFilename)
{
    // Read the vertex shader file
    std::vector<char> vertexShader;
    if (!getFileContents(vertexShaderFilename, vertexShader))
    {
        err() << "Failed to open vertex shader file \"" << vertexShaderFilename << "\"" << std::endl;
        return false;
    }

    // Read the geometry shader file
    std::vector<char> geometryShader;
    if (!getFileContents(geometryShaderFilename, geometryShader))
    {
        err() << "Failed to open geometry shader file \"" << geometryShaderFilename << "\"" << std::endl;
        return false;
    }

    // Read the fragment shader file
    std::vector<char> fragmentShader;
    if (!getFileContents(fragmentShaderFilename, fragmentShader))
    {
        err() << "Failed to open fragment shader file \"" << fragmentShaderFilename << "\"" << std::endl;
        return false;
    }

    // Compile the shader program
    return compile(&vertexShader[0], &geometryShader[0], &fragmentShader[0]);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const std::string& shader, Type type)
{
    // Compile the shader program
    if (type == Vertex)
        return compile(shader.c_str(), NULL, NULL);
    else if (type == Geometry)
        return compile(NULL, shader.c_str(), NULL);
    else
        return compile(NULL, NULL, shader.c_str());
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const std::string& vertexShader, const std::string& fragmentShader)
{
    // Compile the shader program
    return compile(vertexShader.c_str(), NULL, fragmentShader.c_str());
}


////////////////////////////////////////////////////////////
bool Shader::loadFromMemory(const std::string& vertexShader, const std::string& geometryShader, const std::string& fragmentShader)
{
    // Compile the shader program
    return compile(vertexShader.c_str(), geometryShader.c_str(), fragmentShader.c_str());
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& stream, Type type)
{
    // Read the shader code from the stream
    std::vector<char> shader;
    if (!getStreamContents(stream, shader))
    {
        err() << "Failed to read shader from stream" << std::endl;
        return false;
    }

    // Compile the shader program
    if (type == Vertex)
        return compile(&shader[0], NULL, NULL);
    else if (type == Geometry)
        return compile(NULL, &shader[0], NULL);
    else
        return compile(NULL, NULL, &shader[0]);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& vertexShaderStream, InputStream& fragmentShaderStream)
{
    // Read the vertex shader code from the stream
    std::vector<char> vertexShader;
    if (!getStreamContents(vertexShaderStream, vertexShader))
    {
        err() << "Failed to read vertex shader from stream" << std::endl;
        return false;
    }

    // Read the fragment shader code from the stream
    std::vector<char> fragmentShader;
    if (!getStreamContents(fragmentShaderStream, fragmentShader))
    {
        err() << "Failed to read fragment shader from stream" << std::endl;
        return false;
    }

    // Compile the shader program
    return compile(&vertexShader[0], NULL, &fragmentShader[0]);
}


////////////////////////////////////////////////////////////
bool Shader::loadFromStream(InputStream& vertexShaderStream, InputStream& geometryShaderStream, InputStream& fragmentShaderStream)
{
    // Read the vertex shader code from the stream
    std::vector<char> vertexShader;
    if (!getStreamContents(vertexShaderStream, vertexShader))
    {
        err() << "Failed to read vertex shader from stream" << std::endl;
        return false;
    }

    // Read the geometry shader code from the stream
    std::vector<char> geometryShader;
    if (!getStreamContents(geometryShaderStream, geometryShader))
    {
        err() << "Failed to read geometry shader from stream" << std::endl;
        return false;
    }

    // Read the fragment shader code from the stream
    std::vector<char> fragmentShader;
    if (!getStreamContents(fragmentShaderStream, fragmentShader))
    {
        err() << "Failed to read fragment shader from stream" << std::endl;
        return false;
    }

    // Compile the shader program
    return compile(&vertexShader[0], &geometryShader[0], &fragmentShader[0]);
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, float x)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform1f(binder.location, x));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Vec2& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform2f(binder.location, v.x, v.y));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Vec3& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform3f(binder.location, v.x, v.y, v.z));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Vec4& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform4f(binder.location, v.x, v.y, v.z, v.w));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, int x)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform1i(binder.location, x));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Ivec2& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform2i(binder.location, v.x, v.y));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Ivec3& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform3i(binder.location, v.x, v.y, v.z));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Ivec4& v)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform4i(binder.location, v.x, v.y, v.z, v.w));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, bool x)
{
    setUniform(name, static_cast<int>(x));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Bvec2& v)
{
    setUniform(name, Glsl::Ivec2(v));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Bvec3& v)
{
    setUniform(name, Glsl::Ivec3(v));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Bvec4& v)
{
    setUniform(name, Glsl::Ivec4(v));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Mat3& matrix)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniformMatrix3fv(binder.location, 1, GL_FALSE, matrix.array));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Glsl::Mat4& matrix)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniformMatrix4fv(binder.location, 1, GL_FALSE, matrix.array));
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, const Texture& texture)
{
    if (m_shaderProgram)
    {
        TransientContextLock lock;

        // Find the location of the variable in the shader
        int location = getUniformLocation(name);
        if (location != -1)
        {
            // Store the location -> texture mapping
            TextureTable::iterator it = m_textures.find(location);
            if (it == m_textures.end())
            {
                // New entry, make sure there are enough texture units
                if (m_textures.size() + 1 >= getMaxTextureUnits())
                {
                    err() << "Impossible to use texture \"" << name << "\" for shader: all available texture units are used" << std::endl;
                    return;
                }

                m_textures[location] = &texture;
            }
            else
            {
                // Location already used, just replace the texture
                it->second = &texture;
            }
        }
    }
}


////////////////////////////////////////////////////////////
void Shader::setUniform(const std::string& name, CurrentTextureType)
{
    if (m_shaderProgram)
    {
        TransientContextLock lock;

        // Find the location of the variable in the shader
        m_currentTexture = getUniformLocation(name);
    }
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const std::string& name, const float* scalarArray, std::size_t length)
{
    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform1fv(binder.location, static_cast<GLsizei>(length), scalarArray));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const std::string& name, const Glsl::Vec2* vectorArray, std::size_t length)
{
    std::vector<float> contiguous = flatten(vectorArray, length);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform2fv(binder.location, static_cast<GLsizei>(length), &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const std::string& name, const Glsl::Vec3* vectorArray, std::size_t length)
{
    std::vector<float> contiguous = flatten(vectorArray, length);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform3fv(binder.location, static_cast<GLsizei>(length), &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const std::string& name, const Glsl::Vec4* vectorArray, std::size_t length)
{
    std::vector<float> contiguous = flatten(vectorArray, length);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniform4fv(binder.location, static_cast<GLsizei>(length), &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const std::string& name, const Glsl::Mat3* matrixArray, std::size_t length)
{
    const std::size_t matrixSize = 3 * 3;

    std::vector<float> contiguous(matrixSize * length);
    for (std::size_t i = 0; i < length; ++i)
        priv::copyMatrix(matrixArray[i].array, matrixSize, &contiguous[matrixSize * i]);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniformMatrix3fv(binder.location, static_cast<GLsizei>(length), GL_FALSE, &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setUniformArray(const std::string& name, const Glsl::Mat4* matrixArray, std::size_t length)
{
    const std::size_t matrixSize = 4 * 4;

    std::vector<float> contiguous(matrixSize * length);
    for (std::size_t i = 0; i < length; ++i)
        priv::copyMatrix(matrixArray[i].array, matrixSize, &contiguous[matrixSize * i]);

    UniformBinder binder(*this, name);
    if (binder.location != -1)
        glCheck(GLEXT_glUniformMatrix4fv(binder.location, static_cast<GLsizei>(length), GL_FALSE, &contiguous[0]));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x)
{
    setUniform(name, x);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x, float y)
{
    setUniform(name, Glsl::Vec2(x, y));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x, float y, float z)
{
    setUniform(name, Glsl::Vec3(x, y, z));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, float x, float y, float z, float w)
{
    setUniform(name, Glsl::Vec4(x, y, z, w));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Vector2f& v)
{
    setUniform(name, v);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Vector3f& v)
{
    setUniform(name, v);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Color& color)
{
    setUniform(name, Glsl::Vec4(color));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Transform& transform)
{
    setUniform(name, Glsl::Mat4(transform));
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, const Texture& texture)
{
    setUniform(name, texture);
}


////////////////////////////////////////////////////////////
void Shader::setParameter(const std::string& name, CurrentTextureType)
{
    setUniform(name, CurrentTexture);
}


////////////////////////////////////////////////////////////
unsigned int Shader::getNativeHandle() const
{
    return m_shaderProgram;
}


////////////////////////////////////////////////////////////
void Shader::bind(const Shader* shader)
{
    TransientContextLock lock;

    // Make sure that we can use shaders
    if (!isAvailable())
    {
        err() << "Failed to bind or unbind shader: your system doesn't support shaders "
              << "(you should test Shader::isAvailable() before trying to use the Shader class)" << std::endl;
        return;
    }

    if (shader && shader->m_shaderProgram)
    {
        // Enable the program
        glCheck(GLEXT_glUseProgramObject(castToGlHandle(shader->m_shaderProgram)));

        // Bind the textures
        shader->bindTextures();

        // Bind the current texture
        if (shader->m_currentTexture != -1)
            glCheck(GLEXT_glUniform1i(shader->m_currentTexture, 0));
    }
    else
    {
        // Bind no shader
        glCheck(GLEXT_glUseProgramObject(0));
    }
}


////////////////////////////////////////////////////////////
bool Shader::isAvailable()
{
    Lock lock(isAvailableMutex);

    static bool checked = false;
    static bool available = false;

    if (!checked)
    {
        checked = true;

        TransientContextLock contextLock;

        // Make sure that extensions are initialized
        sf::priv::ensureExtensionsInit();

#ifdef SFML_OPENGL_ES
        available = true;
#else
        available = GLEXT_multitexture         &&
                    GLEXT_shading_language_100 &&
                    GLEXT_shader_objects       &&
                    GLEXT_vertex_shader        &&
                    GLEXT_fragment_shader;
#endif

    }

    return available;
}


////////////////////////////////////////////////////////////
bool Shader::isGeometryAvailable()
{
    Lock lock(isAvailableMutex);

    static bool checked = false;
    static bool available = false;

    if (!checked)
    {
        checked = true;

        TransientContextLock contextLock;

        // Make sure that extensions are initialized
        sf::priv::ensureExtensionsInit();

#ifdef SFML_OPENGL_ES
        available = false;
#else
        available = isAvailable() && (GLEXT_geometry_shader4 || GLEXT_GL_VERSION_3_2);
#endif

    }

    return available;
}

////////////////////////////////////////////////////////////
const Shader& Shader::getDefaultShader()
{
    static Shader instance;
    static bool first = true;

    if (first)
    {
#ifdef SFML_OPENGL_ES
        instance.loadFromMemory(
            "#version 100\n"
            "attribute vec2 position;"
            "attribute vec4 color;"
            "varying vec4 sf_color;"
            "uniform mat4 sf_modelview;"
            "uniform mat4 sf_projection;"
            "void main()"
            "{"
            "    vec2 pos = position;"
            "    sf_color = color;"
            "    gl_Position = sf_projection * sf_modelview * vec4(pos.xy, 0.0, 1.0);"
            "}",

            "#version 100\n"
            "precision mediump float;"
            "varying vec4 sf_color;"
            "void main()"
            "{"
            "    gl_FragColor = sf_color;"
            "}"
        );
#else
        instance.loadFromMemory(
            "#version 120\n"
            "void main()"
            "{"
            "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
            "    gl_FrontColor = gl_Color;"
            "}",

            "#version 120\n"
            "void main()"
            "{"
            "    gl_FragColor = gl_Color;"
            "}"
        );
#endif
        first = false;
    }

    return instance;
}


////////////////////////////////////////////////////////////
const Shader& Shader::getDefaultTexShader()
{
    static Shader instance;
    static bool first = true;

    if (first)
    {
#ifdef SFML_OPENGL_ES
        instance.loadFromMemory(
            "#version 100\n"
            "attribute vec2 position;"
            "attribute vec4 color;"
            "attribute vec2 texCoord;"
            "varying vec4 sf_color;"
            "varying vec2 sf_texCoord;"
            "uniform mat4 sf_modelview;"
            "uniform mat4 sf_projection;"
            "void main()"
            "{"
            "    vec2 pos = position;"
            "    sf_color = color;"
            "    sf_texCoord = texCoord;"
            "    gl_Position = sf_projection * sf_modelview * vec4(pos.xy, 0.0, 1.0);"
            "}",

            "#version 100\n"
            "precision mediump float;"
            "varying vec4 sf_color;"
            "varying vec2 sf_texCoord;"
            "uniform sampler2D sf_sampler;"
            "uniform mat4 sf_texture;"
            "void main()"
            "{"
            "    vec4 coord = sf_texture * vec4(sf_texCoord, 0.0, 1.0);"
            "    gl_FragColor = texture2D(sf_sampler, coord.xy) * sf_color;"
            "}"
        );
#else
        instance.loadFromMemory(
            "#version 120\n"
            "void main()"
            "{"
            "    gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;"
            "    gl_TexCoord[0] = gl_TextureMatrix[0] * gl_MultiTexCoord0;"
            "    gl_FrontColor = gl_Color;"
            "}",

            "#version 120\n"
            "uniform sampler2D texture;"
            "void main()"
            "{"
            "    vec4 pixel = texture2D(texture, gl_TexCoord[0].xy);"
            "    gl_FragColor = gl_Color * pixel;"
            "}"
        );
#endif
        first = false;
    }

    return instance;
}


////////////////////////////////////////////////////////////
bool Shader::compile(const char* vertexShaderCode, const char* geometryShaderCode, const char* fragmentShaderCode)
{
    TransientContextLock lock;

    // First make sure that we can use shaders
    if (!isAvailable())
    {
        err() << "Failed to create a shader: your system doesn't support shaders "
              << "(you should test Shader::isAvailable() before trying to use the Shader class)" << std::endl;
        return false;
    }

    // Make sure we can use geometry shaders
    if (geometryShaderCode && !isGeometryAvailable())
    {
        err() << "Failed to create a shader: your system doesn't support geometry shaders "
              << "(you should test Shader::isGeometryAvailable() before trying to use geometry shaders)" << std::endl;
        return false;
    }

    // Destroy the shader if it was already created
    if (m_shaderProgram)
    {
        glCheck(GLEXT_glDeleteProgram(castToGlHandle(m_shaderProgram)));
        m_shaderProgram = 0;
    }

    // Reset the internal state
    m_currentTexture = -1;
    m_textures.clear();
    m_uniforms.clear();

    // Create the program
    GLEXT_GLhandle shaderProgram;
    glCheck(shaderProgram = GLEXT_glCreateProgramObject());

    // Create the vertex shader if needed
    if (vertexShaderCode)
    {
        // Create and compile the shader
        GLEXT_GLhandle vertexShader;
        glCheck(vertexShader = GLEXT_glCreateShaderObject(GLEXT_GL_VERTEX_SHADER));
        glCheck(GLEXT_glShaderSource(vertexShader, 1, &vertexShaderCode, NULL));
        glCheck(GLEXT_glCompileShader(vertexShader));

        // Check the compile log
        GLint success;
        glCheck(GLEXT_glGetShaderParameteriv(vertexShader, GLEXT_GL_OBJECT_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            char log[1024];
            glCheck(GLEXT_glGetShaderInfoLog(vertexShader, sizeof(log), 0, log));
            err() << "Failed to compile vertex shader:" << std::endl
                  << log << std::endl;
            glCheck(GLEXT_glDeleteShader(vertexShader));
            glCheck(GLEXT_glDeleteProgram(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(GLEXT_glAttachShader(shaderProgram, vertexShader));
        glCheck(GLEXT_glDeleteShader(vertexShader));
    }

#ifndef SFML_OPENGL_ES
    // Create the geometry shader if needed
    if (geometryShaderCode)
    {
        // Create and compile the shader
        GLEXT_GLhandle geometryShader = GLEXT_glCreateShaderObject(GLEXT_GL_GEOMETRY_SHADER);
        glCheck(GLEXT_glShaderSource(geometryShader, 1, &geometryShaderCode, NULL));
        glCheck(GLEXT_glCompileShader(geometryShader));

        // Check the compile log
        GLint success;
        glCheck(GLEXT_glGetShaderParameteriv(geometryShader, GLEXT_GL_OBJECT_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            char log[1024];
            glCheck(GLEXT_glGetShaderInfoLog(geometryShader, sizeof(log), 0, log));
            err() << "Failed to compile geometry shader:" << std::endl
                  << log << std::endl;

            glCheck(GLEXT_glDeleteShader(geometryShader));
            glCheck(GLEXT_glDeleteProgram(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(GLEXT_glAttachShader(shaderProgram, geometryShader));
        glCheck(GLEXT_glDeleteShader(geometryShader));
    }
#endif
    // Create the fragment shader if needed
    if (fragmentShaderCode)
    {
        // Create and compile the shader
        GLEXT_GLhandle fragmentShader;
        glCheck(fragmentShader = GLEXT_glCreateShaderObject(GLEXT_GL_FRAGMENT_SHADER));
        glCheck(GLEXT_glShaderSource(fragmentShader, 1, &fragmentShaderCode, NULL));
        glCheck(GLEXT_glCompileShader(fragmentShader));

        // Check the compile log
        GLint success;
        glCheck(GLEXT_glGetShaderParameteriv(fragmentShader, GLEXT_GL_OBJECT_COMPILE_STATUS, &success));
        if (success == GL_FALSE)
        {
            char log[1024];
            glCheck(GLEXT_glGetShaderInfoLog(fragmentShader, sizeof(log), 0, log));
            err() << "Failed to compile fragment shader:" << std::endl
                  << log << std::endl;
            glCheck(GLEXT_glDeleteShader(fragmentShader));
            glCheck(GLEXT_glDeleteProgram(shaderProgram));
            return false;
        }

        // Attach the shader to the program, and delete it (not needed anymore)
        glCheck(GLEXT_glAttachShader(shaderProgram, fragmentShader));
        glCheck(GLEXT_glDeleteShader(fragmentShader));
    }

    // Link the program
    glCheck(GLEXT_glLinkProgram(shaderProgram));

    // Check the link log
    GLint success;
    glCheck(GLEXT_glGetProgramParameteriv(shaderProgram, GLEXT_GL_OBJECT_LINK_STATUS, &success));
    if (success == GL_FALSE)
    {
        char log[1024];
        glCheck(GLEXT_glGetProgramInfoLog(shaderProgram, sizeof(log), 0, log));
        err() << "Failed to link shader:" << std::endl
              << log << std::endl;
        glCheck(GLEXT_glDeleteProgram(shaderProgram));
        return false;
    }


    m_shaderProgram = castFromGlHandle(shaderProgram);

    // Force an OpenGL flush, so that the shader will appear updated
    // in all contexts immediately (solves problems in multi-threaded apps)
    glCheck(glFlush());

    return true;
}


////////////////////////////////////////////////////////////
void Shader::bindTextures() const
{
    TextureTable::const_iterator it = m_textures.begin();
    for (std::size_t i = 0; i < m_textures.size(); ++i)
    {
        GLint index = static_cast<GLsizei>(i + 1);
        glCheck(GLEXT_glUniform1i(it->first, index));
        glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0 + static_cast<GLenum>(index)));
        Texture::bind(it->second);
        ++it;
    }

    // Make sure that the texture unit which is left active is the number 0
    glCheck(GLEXT_glActiveTexture(GLEXT_GL_TEXTURE0));
}


////////////////////////////////////////////////////////////
int Shader::getUniformLocation(const std::string& name)
{
    // Check the cache
    UniformTable::const_iterator it = m_uniforms.find(name);
    if (it != m_uniforms.end())
    {
        // Already in cache, return it
        return it->second;
    }
    else
    {
        // Not in cache, request the location from OpenGL
        int location = GLEXT_glGetUniformLocation(castToGlHandle(m_shaderProgram), name.c_str());
        m_uniforms.insert(std::make_pair(name, location));

        if (location == -1)
            err() << "Uniform \"" << name << "\" not found in shader" << std::endl;

        return location;
    }
}

} // namespace sf
