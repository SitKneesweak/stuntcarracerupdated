#ifndef _GL_LOADER_H_
#define _GL_LOADER_H_

/*	--------------------------------------------------------------------------------------- */
/*	Portable OpenGL entry-point loader.														*/
/*																							*/
/*	Everything past GL 1.1 has to be fetched at runtime rather than linked against:			*/
/*	Windows' opengl32 only exports the 1.1 set, and a plain <GL/gl.h> on Linux declares		*/
/*	no more than that either. macOS does export the full 2.1 set from the framework, but	*/
/*	we go through the same path there so there is exactly one code path to reason about.	*/
/*																							*/
/*	The pointers are named scr_glFoo and then #defined over the plain glFoo names, the		*/
/*	usual GLEW trick. This header is included *after* <GL/gl.h> (or its equivalent), so		*/
/*	any real declaration has already been parsed by the time the macro takes effect and		*/
/*	only call sites get rewritten.															*/
/*																							*/
/*	Call SCR_LoadGLProcs() once, after the GL context is current. SCR_HaveGLShaders()		*/
/*	then reports whether the GL 2.0 shader set came back - drivers that predate it still		*/
/*	run the game, just without the fog and sharp-pixel filtering.							*/
/*	--------------------------------------------------------------------------------------- */

#ifndef HAVE_GLES

#ifndef APIENTRY
#define APIENTRY
#endif
#define SCR_GLAPI APIENTRY

#include <stddef.h>
#include <stdbool.h>

/*	Tokens missing from a 1.1-era <GL/gl.h>. Guarded so a fuller header wins. */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE		0x812F
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0				0x84C0
#endif
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE			0x809D
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER		0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER		0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS		0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS			0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH		0x8B84
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*	Resolve everything. Safe to call more than once; returns SCR_HaveGLShaders(). */
bool SCR_LoadGLProcs(void);

/*	True once the full GL 2.0 shader set resolved. False means fixed-function only. */
bool SCR_HaveGLShaders(void);

/*	GL 1.3 - multitexture. */
typedef void   (SCR_GLAPI *SCR_PFN_ActiveTexture)(GLenum);
typedef void   (SCR_GLAPI *SCR_PFN_ClientActiveTexture)(GLenum);

/*	GL 2.0 - shader objects. char* rather than GLchar*, which 1.1 headers lack. */
typedef GLuint (SCR_GLAPI *SCR_PFN_CreateShader)(GLenum);
typedef void   (SCR_GLAPI *SCR_PFN_ShaderSource)(GLuint, GLsizei, const char* const*, const GLint*);
typedef void   (SCR_GLAPI *SCR_PFN_CompileShader)(GLuint);
typedef void   (SCR_GLAPI *SCR_PFN_GetShaderiv)(GLuint, GLenum, GLint*);
typedef void   (SCR_GLAPI *SCR_PFN_GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, char*);
typedef void   (SCR_GLAPI *SCR_PFN_DeleteShader)(GLuint);
typedef GLuint (SCR_GLAPI *SCR_PFN_CreateProgram)(void);
typedef void   (SCR_GLAPI *SCR_PFN_AttachShader)(GLuint, GLuint);
typedef void   (SCR_GLAPI *SCR_PFN_LinkProgram)(GLuint);
typedef void   (SCR_GLAPI *SCR_PFN_GetProgramiv)(GLuint, GLenum, GLint*);
typedef void   (SCR_GLAPI *SCR_PFN_GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, char*);
typedef void   (SCR_GLAPI *SCR_PFN_DeleteProgram)(GLuint);
typedef void   (SCR_GLAPI *SCR_PFN_UseProgram)(GLuint);
typedef GLint  (SCR_GLAPI *SCR_PFN_GetUniformLocation)(GLuint, const char*);
typedef void   (SCR_GLAPI *SCR_PFN_Uniform1i)(GLint, GLint);
typedef void   (SCR_GLAPI *SCR_PFN_Uniform1f)(GLint, GLfloat);
typedef void   (SCR_GLAPI *SCR_PFN_Uniform2f)(GLint, GLfloat, GLfloat);
typedef void   (SCR_GLAPI *SCR_PFN_Uniform3f)(GLint, GLfloat, GLfloat, GLfloat);
typedef void   (SCR_GLAPI *SCR_PFN_UniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);

extern SCR_PFN_ActiveTexture		scr_glActiveTexture;
extern SCR_PFN_ClientActiveTexture	scr_glClientActiveTexture;
extern SCR_PFN_CreateShader			scr_glCreateShader;
extern SCR_PFN_ShaderSource			scr_glShaderSource;
extern SCR_PFN_CompileShader		scr_glCompileShader;
extern SCR_PFN_GetShaderiv			scr_glGetShaderiv;
extern SCR_PFN_GetShaderInfoLog		scr_glGetShaderInfoLog;
extern SCR_PFN_DeleteShader			scr_glDeleteShader;
extern SCR_PFN_CreateProgram		scr_glCreateProgram;
extern SCR_PFN_AttachShader			scr_glAttachShader;
extern SCR_PFN_LinkProgram			scr_glLinkProgram;
extern SCR_PFN_GetProgramiv			scr_glGetProgramiv;
extern SCR_PFN_GetProgramInfoLog	scr_glGetProgramInfoLog;
extern SCR_PFN_DeleteProgram		scr_glDeleteProgram;
extern SCR_PFN_UseProgram			scr_glUseProgram;
extern SCR_PFN_GetUniformLocation	scr_glGetUniformLocation;
extern SCR_PFN_Uniform1i			scr_glUniform1i;
extern SCR_PFN_Uniform1f			scr_glUniform1f;
extern SCR_PFN_Uniform2f			scr_glUniform2f;
extern SCR_PFN_Uniform3f			scr_glUniform3f;
extern SCR_PFN_UniformMatrix4fv		scr_glUniformMatrix4fv;

#ifdef __cplusplus
}
#endif

#define glActiveTexture			scr_glActiveTexture
#define glClientActiveTexture	scr_glClientActiveTexture
#define glCreateShader			scr_glCreateShader
#define glShaderSource			scr_glShaderSource
#define glCompileShader			scr_glCompileShader
#define glGetShaderiv			scr_glGetShaderiv
#define glGetShaderInfoLog		scr_glGetShaderInfoLog
#define glDeleteShader			scr_glDeleteShader
#define glCreateProgram			scr_glCreateProgram
#define glAttachShader			scr_glAttachShader
#define glLinkProgram			scr_glLinkProgram
#define glGetProgramiv			scr_glGetProgramiv
#define glGetProgramInfoLog		scr_glGetProgramInfoLog
#define glDeleteProgram			scr_glDeleteProgram
#define glUseProgram			scr_glUseProgram
#define glGetUniformLocation	scr_glGetUniformLocation
#define glUniform1i				scr_glUniform1i
#define glUniform1f				scr_glUniform1f
#define glUniform2f				scr_glUniform2f
#define glUniform3f				scr_glUniform3f
#define glUniformMatrix4fv		scr_glUniformMatrix4fv

#else	/* HAVE_GLES: GLES1 has no shaders, and glActiveTexture is in the core lib. */

#ifdef __cplusplus
extern "C" {
#endif
inline bool SCR_LoadGLProcs(void) { return false; }
inline bool SCR_HaveGLShaders(void) { return false; }
#ifdef __cplusplus
}
#endif

#endif	/* HAVE_GLES */

#endif	/* _GL_LOADER_H_ */
