/*	--------------------------------------------------------------------------------------- */
/*	Portable OpenGL entry-point loader - see the block comment in gl_loader.h.				*/
/*	--------------------------------------------------------------------------------------- */

#include "dx_linux.h"

#ifndef HAVE_GLES

SCR_PFN_ActiveTexture		scr_glActiveTexture			= NULL;
SCR_PFN_ClientActiveTexture	scr_glClientActiveTexture	= NULL;
SCR_PFN_CreateShader		scr_glCreateShader			= NULL;
SCR_PFN_ShaderSource		scr_glShaderSource			= NULL;
SCR_PFN_CompileShader		scr_glCompileShader			= NULL;
SCR_PFN_GetShaderiv			scr_glGetShaderiv			= NULL;
SCR_PFN_GetShaderInfoLog	scr_glGetShaderInfoLog		= NULL;
SCR_PFN_DeleteShader		scr_glDeleteShader			= NULL;
SCR_PFN_CreateProgram		scr_glCreateProgram			= NULL;
SCR_PFN_AttachShader		scr_glAttachShader			= NULL;
SCR_PFN_LinkProgram			scr_glLinkProgram			= NULL;
SCR_PFN_GetProgramiv		scr_glGetProgramiv			= NULL;
SCR_PFN_GetProgramInfoLog	scr_glGetProgramInfoLog		= NULL;
SCR_PFN_DeleteProgram		scr_glDeleteProgram			= NULL;
SCR_PFN_UseProgram			scr_glUseProgram			= NULL;
SCR_PFN_GetUniformLocation	scr_glGetUniformLocation	= NULL;
SCR_PFN_Uniform1i			scr_glUniform1i				= NULL;
SCR_PFN_Uniform1f			scr_glUniform1f				= NULL;
SCR_PFN_Uniform2f			scr_glUniform2f				= NULL;
SCR_PFN_Uniform3f			scr_glUniform3f				= NULL;
SCR_PFN_UniformMatrix4fv	scr_glUniformMatrix4fv		= NULL;

static bool sLoaded      = false;
static bool sHaveShaders = false;

/*	SDL_GL_GetProcAddress returns void*; the round trip through a function-pointer type is
	the pragmatic cast everyone uses here. missing[] collects names so one failed extension
	prints one line rather than twenty. */
static void* GetProc(const char* name, const char** missing, int* nMissing)
{
	void* p = SDL_GL_GetProcAddress(name);
	if (!p && *nMissing < 32)
		missing[(*nMissing)++] = name;
	return p;
}

bool SCR_HaveGLShaders(void)
{
	return sHaveShaders;
}

bool SCR_LoadGLProcs(void)
{
	if (sLoaded)
		return sHaveShaders;
	sLoaded = true;

	const char* missing[32];
	int nMissing = 0;

	#define SCR_GET(fn, type) scr_gl##fn = (type)GetProc("gl" #fn, missing, &nMissing)

	SCR_GET(ActiveTexture,			SCR_PFN_ActiveTexture);
	SCR_GET(ClientActiveTexture,	SCR_PFN_ClientActiveTexture);

	/*	Everything below is the GL 2.0 shader set that the fog and sharp-pixel
		filtering need. Nothing else in the shim depends on it. */
	const int firstShaderProc = nMissing;

	SCR_GET(CreateShader,			SCR_PFN_CreateShader);
	SCR_GET(ShaderSource,			SCR_PFN_ShaderSource);
	SCR_GET(CompileShader,			SCR_PFN_CompileShader);
	SCR_GET(GetShaderiv,			SCR_PFN_GetShaderiv);
	SCR_GET(GetShaderInfoLog,		SCR_PFN_GetShaderInfoLog);
	SCR_GET(DeleteShader,			SCR_PFN_DeleteShader);
	SCR_GET(CreateProgram,			SCR_PFN_CreateProgram);
	SCR_GET(AttachShader,			SCR_PFN_AttachShader);
	SCR_GET(LinkProgram,			SCR_PFN_LinkProgram);
	SCR_GET(GetProgramiv,			SCR_PFN_GetProgramiv);
	SCR_GET(GetProgramInfoLog,		SCR_PFN_GetProgramInfoLog);
	SCR_GET(DeleteProgram,			SCR_PFN_DeleteProgram);
	SCR_GET(UseProgram,				SCR_PFN_UseProgram);
	SCR_GET(GetUniformLocation,		SCR_PFN_GetUniformLocation);
	SCR_GET(Uniform1i,				SCR_PFN_Uniform1i);
	SCR_GET(Uniform1f,				SCR_PFN_Uniform1f);
	SCR_GET(Uniform2f,				SCR_PFN_Uniform2f);
	SCR_GET(Uniform3f,				SCR_PFN_Uniform3f);
	SCR_GET(UniformMatrix4fv,		SCR_PFN_UniformMatrix4fv);

	#undef SCR_GET

	sHaveShaders = (nMissing == firstShaderProc);

	if (nMissing) {
		printf("OpenGL: %d entry point(s) unavailable:", nMissing);
		for (int i = 0; i < nMissing; i++)
			printf(" %s", missing[i]);
		printf("\n");
	}
	if (!sHaveShaders)
		printf("OpenGL: no GL 2.0 shader support - fog and sharp-pixel filtering disabled\n");

	const char* version = (const char*)glGetString(GL_VERSION);
	const char* renderer = (const char*)glGetString(GL_RENDERER);
	printf("OpenGL %s on %s\n", version ? version : "?", renderer ? renderer : "?");
	fflush(stdout);

	return sHaveShaders;
}

#endif	/* HAVE_GLES */
