#ifdef linux
#include "dx_linux.h"
// use a light version of stb_image
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

extern bool wideScreen;

const char* BitMapRessourceName(const char* name)
{
static const char* resname[] = {
	"RoadYellowDark", "RoadYellowLight", "RoadRedDark", 
	"RoadRedLight", "RoadBlack", "RoadWhite", 
	0};
static const char* filename[] = {
	"Bitmap/RoadYellowDark.bmp", "Bitmap/RoadYellowLight.bmp", "Bitmap/RoadRedDark.bmp", 
	"Bitmap/RoadRedLight.bmp", "Bitmap/RoadBlack.bmp", "Bitmap/RoadWhite.bmp", 
	0};
	
	int i = 0;
	while(resname[i] && strcmp(resname[i], name)) i++;
	if (filename[i] == 0)
		return name;
	return filename[i];
}

void IDirect3DTexture9::LoadTexture(const char* name) 
{
	if (texID) glDeleteTextures(1, &texID);
	glGenTextures(1, &texID);
	int x,y,n;
	unsigned char *img = stbi_load(BitMapRessourceName(name), &x, &y, &n, 0);
	if(!img) {
		printf("Warning, image \"%s\" => \"%s\" not loaded\n", name, BitMapRessourceName(name));
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		return;
	}
	GLint intfmt = n;
	GLenum fmt = GL_RGBA;
	switch (intfmt) {
    case 1:
        fmt = GL_ALPHA;
        break;
    case 3:     // no alpha channel
		fmt = GL_RGB;
        break;
    case 4:     // contains an alpha channel
		fmt = GL_RGBA;
        break;
	}
	w2 = w = x;
	h2 = h = y;
	// will handle non-pot2 texture later? or resize the texture to POT?
	/*w2 = NP2(w);
	h2 = NP2(h);
	wf = (float)w2 / (float)w;
	hf = (float)h2 / (float)h;*/
	Bind();
	// ugly... Just blindly load the texture without much check!
	glTexParameteri(GL_TEXTURE_2D , GL_TEXTURE_MIN_FILTER , GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER , GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D , GL_TEXTURE_WRAP_S , GL_CLAMP_TO_EDGE );
	glTexParameteri(GL_TEXTURE_2D , GL_TEXTURE_WRAP_T , GL_CLAMP_TO_EDGE );
	glTexImage2D(GL_TEXTURE_2D, 0, intfmt, w2, h2, 0, fmt, GL_UNSIGNED_BYTE, NULL);
	// simple and hugly way to make the texture upside down...
	int pitch = y*n;
	for (int i = 0; i< h ; i++) {
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (h-1)-i, w, 1, fmt, GL_UNSIGNED_BYTE, img+(pitch*i));
	}
	UnBind();
	if (img) free(img);
}

void IDirect3DTexture9::CreateFromMemory(const unsigned char* pixels, int width, int height,
                                         int channels, bool nearest, bool repeatV)
{
	if (texID) glDeleteTextures(1, &texID);
	glGenTextures(1, &texID);

	GLenum fmt = GL_RGBA;
	switch (channels) {
	case 1: fmt = GL_ALPHA; break;
	case 3: fmt = GL_RGB;   break;
	case 4: fmt = GL_RGBA;  break;
	}

	w2 = w = width;
	h2 = h = height;
	wf = hf = 1.0f;

	Bind();
	const GLint filter = nearest ? GL_NEAREST : GL_LINEAR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, repeatV ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	// Rows are tightly packed and the width isn't necessarily a multiple of 4.
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, channels, width, height, 0, fmt, GL_UNSIGNED_BYTE, pixels);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	UnBind();
}


struct sound_buffer_t {
	ALuint id;
};

struct sound_source_t {
	ALuint id;
	ALuint buffer;
	bool playing;
};

sound_buffer_t * sound_load(void* data, int size, int bits, int sign, int channels, int freq);
sound_source_t * sound_source( sound_buffer_t * buffer );
void sound_play( sound_source_t * s );
void sound_play_looping( sound_source_t * s );
bool sound_is_playing( sound_source_t * s );
void sound_stop( sound_source_t * s );
void sound_release_source( sound_source_t * s );
void sound_release_buffer( sound_buffer_t * s );
void sound_set_frequency( sound_source_t * source, long frequency );
void sound_set_pitch( sound_source_t * s, float pitch );
void sound_volume( sound_source_t * s, long decibels );
void sound_pan( sound_source_t * s, long pan );
void sound_position( sound_source_t * s, float x, float y, float z, float min_distance, float max_distance );

void sound_set_position( sound_source_t * s, long newpos );
long sound_get_position( sound_source_t * s );

int npot(int n) {
	int i= 1;
	while(i<n) i<<=1;
	return i;
}

IDirectSoundBuffer8::IDirectSoundBuffer8()
{
	source = NULL;
	buffer = NULL;
}

HRESULT IDirectSoundBuffer8::SetVolume(LONG lVolume)
{
	if (!source)
		return DSERR_GENERIC;
	sound_volume(source, lVolume); 
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags) 
{
	if (!source)
		return DSERR_GENERIC;
	if (dwFlags&DSBPLAY_LOOPING) 
		sound_play_looping(source); 
	else 
		sound_play(source); 
	return DS_OK;
}
  
HRESULT IDirectSoundBuffer8::SetFrequency(DWORD dwFrequency)
{
	if (!source)
		return DSERR_GENERIC;
	sound_set_frequency(source, dwFrequency); 
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::SetCurrentPosition(DWORD dwNewPosition)
{
	if (!source)
		return DSERR_GENERIC;
	sound_set_position(source, dwNewPosition); 
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::GetCurrentPosition(LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor)
{
	if (!source)
		return DSERR_GENERIC;
	if (pdwCurrentPlayCursor)
		*pdwCurrentPlayCursor = sound_get_position(source);
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::Stop() 
{
	if (!source)
		return DSERR_GENERIC;
	sound_stop(source); 
	return DS_OK;
}

HRESULT IDirectSoundBuffer8::SetPan(LONG lPan)
{
	if (!source)
		return DSERR_GENERIC;
#warning TODO: conversion lPan to OpenAL panning
	sound_pan(source, lPan); 
	return DS_OK;
}

IDirectSoundBuffer8::~IDirectSoundBuffer8()
{
	if (buffer)
		Release();
}

HRESULT IDirectSoundBuffer8::Release()
{
	if(source) {
		sound_release_source(source);
		source = NULL;
	}
	if(buffer) {
		sound_release_buffer(buffer);
		buffer = NULL;
	}
	return S_OK;
}

HRESULT IDirectSoundBuffer8::Lock(DWORD dwOffset, DWORD dwBytes, LPVOID * ppvAudioPtr1, LPDWORD  pdwAudioBytes1, LPVOID * ppvAudioPtr2, LPDWORD pdwAudioBytes2, DWORD dwFlags)
{
	if(dwOffset != 0) return E_FAIL;
	*ppvAudioPtr2 = NULL;
	*pdwAudioBytes2 = 0;
	*ppvAudioPtr1 = malloc(dwBytes);
	*pdwAudioBytes1 = dwBytes;
	return S_OK;
}
HRESULT IDirectSoundBuffer8::Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2)
{
	if(dwAudioBytes2!=0) return E_FAIL;
	if(source || buffer) Release();
	buffer = sound_load(pvAudioPtr1, dwAudioBytes1, 8, 0, 1, 11025);
	source = sound_source(buffer);
	free(pvAudioPtr1);
	return S_OK;
}


HRESULT IDirectSound8::CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER * ppDSBuffer, LPUNKNOWN pUnkOuter)
{
	IDirectSoundBuffer8 *tmp = new IDirectSoundBuffer8();
	*ppDSBuffer = tmp;
	return S_OK;
}

HRESULT DirectSoundCreate8(LPCGUID lpcGuidDevice, LPDIRECTSOUND8 * ppDS8, LPUNKNOWN pUnkOuter)
{
	*ppDS8 = new IDirectSound8();
	return DS_OK;
}

/*
 * Matrix
*/
// Try to keep everything column-major to make OpenGL happy...

D3DXMATRIX* D3DXMatrixIdentity(D3DXMATRIX* pOut)
{
#ifdef USEGLM
	*pOut = glm::mat4(1.0f);
#else
	set_identity(pOut->m);
#endif
	return pOut;
}

D3DXMATRIX* D3DXMatrixRotationX(D3DXMATRIX* pOut, FLOAT Angle)
{
#ifdef USEGLM
	*pOut = glm::rotate(glm::mat4(1.0f), Angle, glm::vec3(1.0f, 0.0f, 0.0f));
#else
	matrix_rot(Angle, 1.0f, 0.0f, 0.0f, pOut->m);
#endif
	return pOut;
}
D3DXMATRIX* D3DXMatrixRotationY(D3DXMATRIX* pOut, FLOAT Angle)
{
#ifdef USEGLM
	*pOut = glm::rotate(glm::mat4(1.0f), Angle, glm::vec3(0.0f, 1.0f, 0.0f));
#else
	matrix_rot(Angle, 0.0f, 1.0f, 0.0f, pOut->m);
#endif
	return pOut;
}

D3DXMATRIX* D3DXMatrixRotationZ(D3DXMATRIX* pOut, FLOAT Angle)
{
#ifdef USEGLM
	*pOut = glm::rotate(glm::mat4(1.0f), Angle, glm::vec3(0.0f, 0.0f, 1.0f));
#else
	matrix_rot(Angle, 0.0f, 0.0f, 1.0f, pOut->m);
#endif
	return pOut;
}

D3DXMATRIX* D3DXMatrixTranslation(D3DXMATRIX* pOut, FLOAT x, FLOAT y, FLOAT z)
{
#ifdef USEGLM
	*pOut = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
#else
	matrix_trans(x, y, z, pOut->m);
#endif
	return pOut;
}

D3DXMATRIX* D3DXMatrixScaling(D3DXMATRIX *pOut, FLOAT sx, FLOAT sy, FLOAT sz)
{
#ifdef USEGLM
	*pOut = glm::translate(glm::mat4(1.0f), glm::vec3(sx, sy, sz));
#else
	matrix_scale(sx, sy, sz, pOut->m);
#endif
	return pOut;
}


D3DXMATRIX* D3DXMatrixMultiply(D3DXMATRIX* pOut, const D3DXMATRIX* pM1, const D3DXMATRIX* pM2)
{
#ifdef USEGLM
	*pOut=(*pM2)*(*pM1);	// reverse order because of DX -> OpenGL
#else
	matrix_mul(pM1->m, pM2->m, pOut->m);
#endif
	return pOut;
}

#ifdef USEGLM
glm::vec3 FromVector(const D3DXVECTOR3* vec)		
{		
	glm::vec3 ret;		
	ret[0]=vec->x;		
	ret[1]=vec->y;		
	ret[2]=vec->z;		
	return ret;		
}
#endif

D3DXMATRIX* D3DXMatrixLookAtLH(D3DXMATRIX* pOut, const D3DXVECTOR3* pEye, const D3DXVECTOR3* pAt, const D3DXVECTOR3* pUp)
{
#ifdef USEGLM
	glm::vec3 eye=FromVector(pEye);		
 	glm::vec3 at=FromVector(pAt);		
 	glm::vec3 up=FromVector(pUp);
#if 0
	// checked, same as DX9
	glm::vec3 vZ = glm::normalize(at - eye);
	glm::vec3 vX = glm::normalize(glm::cross(up, vZ));
	glm::vec3 vY = glm::cross(vZ, vX);

	*pOut = glm::mat4(	vX.x,			vY.x,			vZ.x,			0.0f,
						vX.y,			vY.y,			vZ.y,			0.0f,
						vX.z,			vY.z,			vZ.z,			0.0f,
						glm::dot(-vX, eye), glm::dot(-vY, eye),	glm::dot(-vZ, eye),	1.0f);
#else
 	*pOut = glm::lookAt(eye, at, up);
#endif
#else
	matrix_lookat(&pEye->x, &pAt->x, &pUp->x, pOut->m);
#endif
	return pOut;
}

D3DXMATRIX* D3DXMatrixPerspectiveFovLH(D3DXMATRIX *pOut, FLOAT fovy, FLOAT Aspect, FLOAT zn, FLOAT zf)
{
#ifdef USEGLM
#if 0
	float yScale = 1.0f / tanf(fovy/2.0f);
	float xScale = yScale / Aspect;
	float right = -xScale, left = +xScale;
	float top = -yScale, bottom = +yScale;

	float x1 = ( 2 * zn ) / ( right - left );
	float z1 = ( right + left ) / ( right - left );
 
	float y2 = ( 2 * zn ) / ( top - bottom );
	float z2 = ( top + bottom ) / ( top - bottom );
 
	float z3 = -( zf + zn ) / ( zf - zn );
	float w3 = -( 2 * zf * zn ) / ( zf - zn );
 
	*pOut = glm::mat4(
		x1,  0.f,   z1, 0.f,
		0.f,  y2,   z2, 0.f,
		0.f, 0.f,   z3,  w3,
		0.f, 0.f, -1.f, 0.f );
#else
	float fw, fh;
	fh = tanf( fovy / 2.0f) * zn;
	fw = fh * Aspect;
	*pOut = glm::frustum(-fw, +fw, +fh, -fh, zn, zf);
#endif
	//*pOut = glm::perspective(fovy, Aspect, zn, zf);
#else
#if 0
 	float yScale = 1.0f / tanf(fovy/2.0f);
 	float xScale = yScale / Aspect;
	float nf = zn - zf;
	pOut->m[0+ 0] = xScale;
	pOut->m[1+ 4] = yScale;
	pOut->m[2+ 8] = (zf+zn)/nf;
	pOut->m[3+ 8] = -1.0f;
	pOut->m[2+12] = 2*zf*zn/nf;
#else
	const float ymax=zn*tanf(fovy*0.5f);
	const float xmax=ymax*Aspect;
	const float temp=2.0f*zn;
	const float temp2=2.0f*xmax;
	const float temp3=2.0f*ymax;
	const float temp4=zf-zn;
	pOut->m[0]=temp/temp2;
	pOut->m[1]=0.0f;
	pOut->m[2]=0.0f;
	pOut->m[3]=0.0f;
	pOut->m[4]=0.0f;
	pOut->m[5]=temp/temp3;
	pOut->m[6]=0.0f;
	pOut->m[7]=0.0f;
	pOut->m[8]=0.0f;
	pOut->m[9]=0.0f;
	pOut->m[10]=zf/temp4;
	pOut->m[11]=1.0f;
	pOut->m[12]=0.0f;
	pOut->m[13]=0.0f;
	pOut->m[14]=(zn*zf)/(zn-zf);
	pOut->m[15]=0.0f;	
#endif
#endif
	return pOut;
}

/*	An off-centre (oblique) frustum, so the projection's principal point can be put on the
	cockpit window's centre rather than the screen's - see SetSceneProjection.
	Same conventions as D3DXMatrixPerspectiveFovLH above, including its flipped y
	(the engine's world y runs down the screen), so b is the screen-TOP extent and t the
	screen-BOTTOM one, exactly as the +fh / -fh arguments are used there.				*/
D3DXMATRIX* D3DXMatrixPerspectiveOffCenterLH(D3DXMATRIX *pOut, FLOAT l, FLOAT r, FLOAT b, FLOAT t, FLOAT zn, FLOAT zf)
{
#ifdef USEGLM
	*pOut = glm::frustum(l, r, b, t, zn, zf);
#else
	pOut->m[0]=(2.0f*zn)/(r-l);
	pOut->m[1]=0.0f;
	pOut->m[2]=0.0f;
	pOut->m[3]=0.0f;
	pOut->m[4]=0.0f;
	pOut->m[5]=(2.0f*zn)/(t-b);
	pOut->m[6]=0.0f;
	pOut->m[7]=0.0f;
	pOut->m[8]=(l+r)/(l-r);
	pOut->m[9]=(t+b)/(b-t);
	pOut->m[10]=zf/(zf-zn);
	pOut->m[11]=1.0f;
	pOut->m[12]=0.0f;
	pOut->m[13]=0.0f;
	pOut->m[14]=(zn*zf)/(zn-zf);
	pOut->m[15]=0.0f;
#endif
	return pOut;
}

#if defined(SCR_FOG_SHADER) || defined(SCR_SHARP_PIXEL)
/*	======================================================================================= */
/*	Shared shader plumbing (used by the fog and the sharp-bilinear 2D filter below).			*/
/*	======================================================================================= */

static GLuint CompileGLShader(GLenum type, const char* source, const char* what)
{
	GLuint shader = glCreateShader(type);
	if (!shader)
		return 0;
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	GLint ok = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (ok == GL_TRUE)
		return shader;

	char log[2048] = {0};
	glGetShaderInfoLog(shader, (GLsizei)sizeof(log), NULL, log);
	printf("%s shader compile failed:\n%s\n", what, log);
	glDeleteShader(shader);
	return 0;
}

// Mirror of the fixed-function colour arg handling further down DrawPrimitive().
int IDirect3DDevice9::ResolveColorMode(bool hasTexture, bool hasColor) const
{
	if (!hasTexture)
		return hasColor ? 1 : 0;

	const UINT op = colorop[0];
	if (op == D3DTOP_MODULATE)
		return hasColor ? 3 : 2;
	if (op == D3DTOP_SELECTARG1)
		return ((colorarg1[0] == D3DTA_DIFFUSE) && hasColor) ? 1 : 2;
	if (op == D3DTOP_SELECTARG2)
		return ((colorarg2[0] == D3DTA_DIFFUSE) && hasColor) ? 1 : 2;

	return hasColor ? 3 : 2;
}
#endif

#ifdef SCR_FOG_SHADER
/*	======================================================================================= */
/*	Volumetric fog - see the block comment in dx_linux.h.									*/
/*	======================================================================================= */

bool  gFogEnabled     = true;
float gFogDensity     = 0.000008f;
float gFogHeightScale = 8.0f;
float gFogSkyColor[3] = { 0.7f, 0.6f, 0.5f };		// warm and dusty, not blue
float gFogMaxAmount   = 1.0f;						// see dx_linux.h

// The sun sits fixed above and behind; looking towards it warms the haze.
static const float FOG_SUN_WORLD_DIR[3] = { 0.0f, 0.70710678f, 0.70710678f };

static const char* kFogVertexShader =
	"#version 120\n"
	"uniform mat4 uModelView;\n"					// mView*mWorld: gl_ModelViewMatrix holds the
	"varying vec4 vColor;\n"						// full MVP (see ActivateWorldMatrix), so the
	"varying vec2 vTexCoord;\n"					// view-space position has to come in separately
	"varying vec3 vViewPos;\n"
	"void main() {\n"
	"  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
	"  vViewPos    = (uModelView * gl_Vertex).xyz;\n"
	"  vColor      = gl_Color;\n"
	"  vTexCoord   = (gl_TextureMatrix[0] * gl_MultiTexCoord0).xy;\n"
	"}\n";

static const char* kFogFragmentShader =
	"#version 120\n"
	"uniform sampler2D uTexture;\n"
	"uniform int   uColorMode;\n"
	"uniform float uFogDensity;\n"
	"uniform float uFogHeightScale;\n"
	"uniform vec3  uFogSkyColor;\n"
	"uniform float uFogMaxAmount;\n"
	"uniform vec3  uSunDirView;\n"
	"uniform vec3  uCameraPos;\n"					// world space
	"uniform vec3  uWorldUpView;\n"				// world +Y pushed into view space
	"varying vec4 vColor;\n"
	"varying vec2 vTexCoord;\n"
	"varying vec3 vViewPos;\n"
	"vec3 applyFog(in vec3 col, in float t, in vec3 rd, in vec3 lig) {\n"
	"  float a = uFogDensity;\n"
	"  float b = uFogDensity * uFogHeightScale;\n"
	"  vec3  ro = uCameraPos;\n"
	// rd is view space and uWorldUpView is world-up in view space, so this dot recovers the
	// ray's true world-vertical component without an inverse-view per fragment.
	"  float rdY = dot(rd, normalize(uWorldUpView));\n"
	"  float safeRdY = (abs(rdY) < 0.0001) ? ((rdY < 0.0) ? -0.0001 : 0.0001) : rdY;\n"
	"  float fogAmount = (a / b) * exp(-ro.y * b) * (1.0 - exp(-t * safeRdY * b)) / safeRdY;\n"
	"  fogAmount = clamp(fogAmount, 0.0, uFogMaxAmount);\n"
	"  float sunAmount = max(dot(rd, lig), 0.0);\n"
	"  vec3  fogColor = mix(uFogSkyColor, vec3(1.0, 0.9, 0.7), pow(sunAmount, 8.0));\n"
	"  return mix(col, fogColor, fogAmount);\n"
	"}\n"
	"void main() {\n"
	"  vec4 outColor = vec4(1.0);\n"
	"  if (uColorMode == 1) {\n"
	"    outColor = vColor;\n"
	"  } else if (uColorMode == 2) {\n"
	"    outColor = texture2D(uTexture, vTexCoord);\n"
	"  } else if (uColorMode == 3) {\n"
	"    outColor = texture2D(uTexture, vTexCoord) * vColor;\n"
	"  }\n"
	"  float t  = length(vViewPos);\n"
	"  vec3  rd = (t > 0.0001) ? (vViewPos / t) : vec3(0.0, 0.0, 1.0);\n"
	"  outColor.rgb = applyFog(outColor.rgb, t, rd, normalize(uSunDirView));\n"
	"  gl_FragColor = outColor;\n"
	"}\n";

bool IDirect3DDevice9::EnsureFogProgram()
{
	if (mFogProgram)
		return true;
	if (mFogTried)
		return false;
	mFogTried = true;

	if (!SCR_HaveGLShaders())
		return false;

	GLuint vs = CompileGLShader(GL_VERTEX_SHADER, kFogVertexShader, "Fog");
	if (!vs)
		return false;
	GLuint fs = CompileGLShader(GL_FRAGMENT_SHADER, kFogFragmentShader, "Fog");
	if (!fs) {
		glDeleteShader(vs);
		return false;
	}

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint linked = GL_FALSE;
	glGetProgramiv(prog, GL_LINK_STATUS, &linked);
	if (linked != GL_TRUE) {
		char log[2048] = {0};
		glGetProgramInfoLog(prog, (GLsizei)sizeof(log), NULL, log);
		printf("Fog shader link failed:\n%s\n", log);
		glDeleteProgram(prog);
		return false;
	}

	mFogProgram        = prog;
	mFogU_ModelView    = glGetUniformLocation(prog, "uModelView");
	mFogU_ColorMode    = glGetUniformLocation(prog, "uColorMode");
	mFogU_Texture      = glGetUniformLocation(prog, "uTexture");
	mFogU_Density      = glGetUniformLocation(prog, "uFogDensity");
	mFogU_HeightScale  = glGetUniformLocation(prog, "uFogHeightScale");
	mFogU_SkyColor     = glGetUniformLocation(prog, "uFogSkyColor");
	mFogU_MaxAmount    = glGetUniformLocation(prog, "uFogMaxAmount");
	mFogU_SunDirView   = glGetUniformLocation(prog, "uSunDirView");
	mFogU_CameraPos    = glGetUniformLocation(prog, "uCameraPos");
	mFogU_WorldUpView  = glGetUniformLocation(prog, "uWorldUpView");

	printf("Fog shader ready (press G to toggle)\n");
	fflush(stdout);
	return true;
}
#endif	// SCR_FOG_SHADER

#ifdef SCR_SHARP_PIXEL
/*	======================================================================================= */
/*	Sharp-bilinear filtering for the 2D art - see the block comment in dx_linux.h.			*/
/*	======================================================================================= */

bool gSharpPixelEnabled = true;

static const char* kSharpVertexShader =
	"#version 120\n"
	"varying vec4 vColor;\n"
	"varying vec2 vTexCoord;\n"
	"void main() {\n"
	"  gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;\n"
	"  vColor      = gl_Color;\n"
	"  vTexCoord   = (gl_TextureMatrix[0] * gl_MultiTexCoord0).xy;\n"
	"}\n";

static const char* kSharpFragmentShader =
	"#version 120\n"
	"uniform sampler2D uTexture;\n"
	"uniform vec2  uTexSize;\n"
	"uniform int   uColorMode;\n"
	"varying vec4 vColor;\n"
	"varying vec2 vTexCoord;\n"
	"void main() {\n"
	"  vec2 texel = vTexCoord * uTexSize;\n"
	// fwidth() is texels covered per output pixel, so its reciprocal is the magnification.
	// Clamped at 1 so minified art just falls back to ordinary bilinear.
	"  vec2 scale = max(1.0 / max(fwidth(texel), vec2(1e-6)), vec2(1.0));\n"
	// Push the sample towards the texel centre, leaving a one-output-pixel ramp across the
	// boundary for the hardware's bilinear to smooth - that ramp is the whole trick.
	"  vec2 base  = floor(texel);\n"
	"  vec2 dist  = fract(texel) - 0.5;\n"
	"  vec2 flat  = 0.5 - 0.5 / scale;\n"
	"  vec2 f     = (dist - clamp(dist, -flat, flat)) * scale + 0.5;\n"
	"  vec4 texel_color = texture2D(uTexture, (base + f) / uTexSize);\n"
	"  vec4 outColor = vec4(1.0);\n"
	"  if (uColorMode == 1) {\n"
	"    outColor = vColor;\n"
	"  } else if (uColorMode == 2) {\n"
	"    outColor = texel_color;\n"
	"  } else if (uColorMode == 3) {\n"
	"    outColor = texel_color * vColor;\n"
	"  }\n"
	"  gl_FragColor = outColor;\n"
	"}\n";

bool IDirect3DDevice9::EnsureSharpProgram()
{
	if (mSharpProgram)
		return true;
	if (mSharpTried)
		return false;
	mSharpTried = true;

	if (!SCR_HaveGLShaders())
		return false;

	GLuint vs = CompileGLShader(GL_VERTEX_SHADER, kSharpVertexShader, "Sharp-pixel");
	if (!vs)
		return false;
	GLuint fs = CompileGLShader(GL_FRAGMENT_SHADER, kSharpFragmentShader, "Sharp-pixel");
	if (!fs) {
		glDeleteShader(vs);
		return false;
	}

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vs);
	glAttachShader(prog, fs);
	glLinkProgram(prog);
	glDeleteShader(vs);
	glDeleteShader(fs);

	GLint linked = GL_FALSE;
	glGetProgramiv(prog, GL_LINK_STATUS, &linked);
	if (linked != GL_TRUE) {
		char log[2048] = {0};
		glGetProgramInfoLog(prog, (GLsizei)sizeof(log), NULL, log);
		printf("Sharp-pixel shader link failed:\n%s\n", log);
		glDeleteProgram(prog);
		return false;
	}

	mSharpProgram      = prog;
	mSharpU_Texture    = glGetUniformLocation(prog, "uTexture");
	mSharpU_TexSize    = glGetUniformLocation(prog, "uTexSize");
	mSharpU_ColorMode  = glGetUniformLocation(prog, "uColorMode");

	printf("Sharp-pixel shader ready (press Y to toggle)\n");
	fflush(stdout);
	return true;
}
#endif	// SCR_SHARP_PIXEL

void IDirect3DDevice9::ActivateWorldMatrix()
{
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
#ifdef USEGLM
	glLoadMatrixf(glm::value_ptr(mInv*mProj*mView*mWorld));
#else
	float m[16];
	matrix_mul(mProj.m, mView.m, m);
	matrix_mul(m, mWorld.m, m);
	glLoadMatrixf(m);
#endif
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
}
void IDirect3DDevice9::DeactivateWorldMatrix()
{
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
}

uint32_t GetStrideFromFVF(DWORD fvf) {
	uint32_t stride = 0;
	if(fvf & D3DFVF_DIFFUSE) stride += sizeof(DWORD);
	if(fvf & D3DFVF_NORMAL) stride +=3*sizeof(float);
	if(fvf & D3DFVF_XYZ) stride +=3*sizeof(float);
	if(fvf & D3DFVF_XYZRHW) stride += 4*sizeof(float);
	if(fvf & D3DFVF_XYZW) stride += 4*sizeof(float);
	if(fvf & D3DFVF_TEX0) stride += 2*sizeof(float);

	return stride;
}

// IDirect3DDevice9
IDirect3DDevice9::IDirect3DDevice9()
{
	for (int i=0; i<8; i++) {
		colorop[i] = 0;
		colorarg1[i] = 0;
		colorarg2[i] = 0;
		alphaop[i] = 0;
	}
#ifdef USEGLM
	mView = glm::mat4(1.0f);
 	mWorld = glm::mat4(1.0f);
 	mProj = glm::mat4(1.0f);
 	mText = glm::mat4(1.0f);
	mInv = 
		glm::mat4(-1, 0, 0, 0,
				   0,-1, 0, 0,
				   0, 0,+1, 0,
				   0, 0, 0, 1);
#else
	set_identity(mView.m);
	set_identity(mWorld.m);
	set_identity(mProj.m);
	set_identity(mText.m);
#endif
}

IDirect3DDevice9::~IDirect3DDevice9()
{
}

HRESULT IDirect3DDevice9::SetTransform(D3DTRANSFORMSTATETYPE State, D3DXMATRIX* pMatrix)
{
	switch (State) 
	{
		case D3DTS_VIEW:
			mView = *pMatrix;
			break;
		case D3DTS_WORLD:
			mWorld = *pMatrix;
			break;
		case D3DTS_PROJECTION:
			mProj = *pMatrix;
			break;
		case D3DTS_TEXTURE0:
		case D3DTS_TEXTURE1:
		case D3DTS_TEXTURE2:
		case D3DTS_TEXTURE3:
		case D3DTS_TEXTURE4:
			//TODO change active texture...
			mText = *pMatrix;
			glMatrixMode(GL_TEXTURE);
#ifdef USEGLM
			glLoadMatrixf(glm::value_ptr(mText));
#else
			glLoadMatrixf(mText.m);
#endif
			break;
		default:
			printf("Unhandled Matrix SetTransform(%X, %p)\n", State, pMatrix);
	}
	return S_OK;
}

HRESULT IDirect3DDevice9::GetTransform(D3DTRANSFORMSTATETYPE State, D3DXMATRIX* pMatrix)
{
	switch (State) 
	{
		case D3DTS_VIEW:
			*pMatrix = mView;
			break;
		case D3DTS_PROJECTION:
			*pMatrix = mProj;
			break;
		case D3DTS_WORLD:
			*pMatrix = mWorld;
			break;
		case D3DTS_TEXTURE0:
		case D3DTS_TEXTURE1:
		case D3DTS_TEXTURE2:
		case D3DTS_TEXTURE3:
		case D3DTS_TEXTURE4:
			#warning TODO change active texture...
			*pMatrix = mText;
			break;
		default:
			printf("Unhandled Matrix SetTransform(%X, %p)\n", State, pMatrix);
	}
	return S_OK;
}

HRESULT IDirect3DDevice9::SetRenderState(D3DRENDERSTATETYPE State, int Value)
{
	switch (State)
	{
		case D3DRS_ZENABLE:
			if(Value) {
				glDepthMask(GL_TRUE);
				glEnable(GL_DEPTH_TEST);
			} else {
				glDepthMask(GL_FALSE);
				glDisable(GL_DEPTH_TEST);
			}
			break;
		case D3DRS_CULLMODE:
			switch(Value)
			{
				case D3DCULL_NONE:
					glDisable(GL_CULL_FACE);
					break;
				case D3DCULL_CW:
					glFrontFace(GL_CW);
					glCullFace(GL_FRONT);
					glEnable(GL_CULL_FACE);
					break;
				case D3DCULL_CCW:
					glFrontFace(GL_CCW);
					glCullFace(GL_FRONT);
					glEnable(GL_CULL_FACE);
					break;
			}
			break;
		case D3DRS_SRCBLENDALPHA:
			//TODO
			break;
		case D3DRS_DESTBLENDALPHA:
			//TODO
			break;
		case D3DRS_ALPHABLENDENABLE:
			if (Value) {
				glEnable(GL_ALPHA_TEST);
			} else {
				glDisable(GL_ALPHA_TEST);
			}
			break;
		case D3DRS_SRCBLEND:
			//TODO
			break;
		case D3DRS_DESTBLEND:
			//TODO
			break;
		default:
			printf("Unhandled Render State %X=%d\n", State, Value);
	}
	return S_OK;
}

HRESULT IDirect3DDevice9::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount)
{
	const GLenum primgl[] = {GL_POINTS, GL_LINES, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN};
	const GLenum prim1[] = {1, 2, 1, 3, 1, 1};
	const GLenum prim2[] = {0, 0, 1, 0, 2, 2};
	if(PrimitiveType<D3DPT_POINTLIST || PrimitiveType>D3DPT_TRIANGLEFAN) {
		printf("Unsupported Primitive %d\n", PrimitiveType);
		return E_FAIL;
	}
	if(PrimitiveCount==0)
		return S_OK;

	GLenum mode = primgl[PrimitiveType-1];
	bool transf = ((fvf & D3DFVF_XYZRHW)==0);
	char* ptr = (char*)buffer[0]->buffer.buffer;
	bool vtx = false, col = false, tex0 = false, tex1 = false;
	if(fvf & D3DFVF_XYZ) {
		glVertexPointer(3, GL_FLOAT, stride[0], ptr);
		ptr+=3*sizeof(float);
		vtx = true;
	};
	if(fvf & D3DFVF_XYZW) {
		glVertexPointer(4, GL_FLOAT, stride[0], ptr);
		ptr+=4*sizeof(float);
		vtx = true;
	};
	if(fvf & D3DFVF_XYZRHW) {
		glVertexPointer(2, GL_FLOAT, stride[0], ptr);
		ptr+=4*sizeof(float);
		vtx = true;
	};
	if(fvf & D3DFVF_DIFFUSE) {
		glColorPointer(4, GL_UNSIGNED_BYTE, stride[0], ptr);
		ptr+=sizeof(DWORD);
		col = true;
	}
	if(fvf & D3DFVF_TEX0) {
		glTexCoordPointer(2, GL_FLOAT, stride[0], ptr);
		ptr+=2*sizeof(float);
		tex0 = true;
	}
	if(fvf & D3DFVF_TEX1) {
		glTexCoordPointer(2, GL_FLOAT, stride[0], ptr);
		ptr+=2*sizeof(float);
		tex1 = true;
	}

	if (vtx)
		glEnableClientState(GL_VERTEX_ARRAY);
	else
		glDisableClientState(GL_VERTEX_ARRAY);

	// handles some fixed pipeline  COLOR arg...
	if((colorop[0]==D3DTOP_SELECTARG1) && (colorarg1[0]!=D3DTA_DIFFUSE))
		col = false;
	if((colorop[0]==D3DTOP_SELECTARG2) && (colorarg2[0]!=D3DTA_DIFFUSE))
		col = false;
/*	if((colorop[0]==D3DTOP_SELECTARG1) && (colorarg1[0]==D3DTA_TEXTURE)) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	} else*/ {
		glDisable(GL_BLEND);
	}
	
	if (col)
		glEnableClientState(GL_COLOR_ARRAY);
	else {
		glDisableClientState(GL_COLOR_ARRAY);
		glColor3f(1.0f,1.0f,1.0f);
	}

	if (tex0 || tex1) {
		if (colorop[0] <= D3DTOP_DISABLE) {
			glDisable(GL_TEXTURE_2D);
			glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			glDisable(GL_BLEND);
		} else {
			glEnable(GL_TEXTURE_2D);
			glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			glEnable(GL_BLEND);
		}
	} else {
		glDisable(GL_TEXTURE_2D);
		glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	if(transf) ActivateWorldMatrix();

#ifdef SCR_FOG_SHADER
	// Fog only applies to world-space geometry - transf is false for the pre-transformed
	// (D3DFVF_XYZRHW) HUD/cockpit quads, and the text helper uses its own glBegin path,
	// so both stay on the fixed-function pipeline untouched.
	const bool useFog = transf && vtx && (fvf & D3DFVF_XYZ) && gFogEnabled && EnsureFogProgram();
	if (useFog) {
		const bool hasTexture = (tex0 || tex1) && (colorop[0] > D3DTOP_DISABLE);
		const glm::mat4 modelView   = mView * mWorld;
		const glm::vec3 cameraWorld = glm::vec3(glm::inverse(mView) * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f));

		glm::vec3 worldUpView = glm::vec3(mView * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f));
		const float upLen = glm::length(worldUpView);
		worldUpView = (upLen > 0.0001f) ? (worldUpView / upLen) : glm::vec3(0.0f, 1.0f, 0.0f);

		const glm::vec3 sunWorld(FOG_SUN_WORLD_DIR[0], FOG_SUN_WORLD_DIR[1], FOG_SUN_WORLD_DIR[2]);
		glm::vec3 sunView = glm::vec3(mView * glm::vec4(sunWorld, 0.0f));
		const float sunLen = glm::length(sunView);
		sunView = (sunLen > 0.0001f) ? (sunView / sunLen) : sunWorld;

		glUseProgram(mFogProgram);
		glUniformMatrix4fv(mFogU_ModelView, 1, GL_FALSE, glm::value_ptr(modelView));
		glUniform1i(mFogU_ColorMode, ResolveColorMode(hasTexture, col));
		glUniform1i(mFogU_Texture, 0);
		glUniform1f(mFogU_Density, gFogDensity);
		glUniform1f(mFogU_HeightScale, gFogHeightScale);
		glUniform3f(mFogU_SkyColor, gFogSkyColor[0], gFogSkyColor[1], gFogSkyColor[2]);
		glUniform1f(mFogU_MaxAmount, gFogMaxAmount);
		glUniform3f(mFogU_SunDirView, sunView.x, sunView.y, sunView.z);
		glUniform3f(mFogU_CameraPos, cameraWorld.x, cameraWorld.y, cameraWorld.z);
		glUniform3f(mFogU_WorldUpView, worldUpView.x, worldUpView.y, worldUpView.z);
	}
#endif

#ifdef SCR_SHARP_PIXEL
	// The mirror image of the fog test: the pre-transformed (D3DFVF_XYZRHW) textured quads
	// are exactly the 2D art - cockpit, menus, win/lose screens - blown up from 320x200-era
	// texels, so they're the ones that need the sharpening.
	const bool useSharp = !transf && vtx && gSharpPixelEnabled &&		// !transf, so never the fog's geometry
						  (tex0 || tex1) && (colorop[0] > D3DTOP_DISABLE) &&
						  mTexture0 && mTexture0->Width() > 0 && mTexture0->Height() > 0 &&
						  EnsureSharpProgram();
	if (useSharp) {
		glUseProgram(mSharpProgram);
		glUniform1i(mSharpU_Texture, 0);
		glUniform2f(mSharpU_TexSize, (float)mTexture0->Width(), (float)mTexture0->Height());
		glUniform1i(mSharpU_ColorMode, ResolveColorMode(true, col));
	}
#endif

	glDrawArrays(mode, StartVertex, prim1[PrimitiveType-1]*PrimitiveCount+prim2[PrimitiveType-1]);

#ifdef SCR_FOG_SHADER
	if (useFog)
		glUseProgram(0);
#endif
#ifdef SCR_SHARP_PIXEL
	if (useSharp)
		glUseProgram(0);
#endif

	if(transf) DeactivateWorldMatrix();
	return S_OK;
}

HRESULT IDirect3DDevice9::SetTextureStageState(DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	if(Stage>7) {
		printf("Unhandled SetTextureStageState(%d, 0x%X, 0x%X)\n", Stage, Type, Value);
		return S_OK;
	}

/*	glActiveTexture(GL_TEXTURE0+Stage);
	glClientActiveTexture(GL_TEXTURE0+Stage);*/

	switch(Type)
	{
		case D3DTSS_COLOROP:
			colorop[Stage] = Value;
/*			switch(Value)
			{
				case D3DTOP_DISABLE:
					glDisable(GL_TEXTURE_2D);
					break;
				case D3DTOP_SELECTARG1:
					if(colorarg1[Stage]==D3DTA_TEXTURE)
						glEnable(GL_TEXTURE_2D);
				case D3DTOP_SELECTARG2:
					if(colorarg2[Stage]==D3DTA_TEXTURE)
						glEnable(GL_TEXTURE_2D);
					break;
				default:
					printf("Unhandled SetTextureStageState(%d, D3DTSS_COLOROP, %d)\n", Stage, Value);
			}*/
			break;
		case D3DTSS_COLORARG1:
			colorarg1[Stage] = Value;
		/*	if(Value==D3DTA_TEXTURE && colorop[Stage]==D3DTOP_SELECTARG1)
				glEnable(GL_TEXTURE_2D);*/
			break;
		case D3DTSS_COLORARG2:
			colorarg2[Stage] = Value;
		/*	if(Value==D3DTA_TEXTURE && colorop[Stage]==D3DTOP_SELECTARG2)
				glEnable(GL_TEXTURE_2D);*/
			break;
		case D3DTSS_ALPHAOP:
			//TODO probably
			break;
		case D3DTSS_ALPHAARG1:
			break;
		case D3DTSS_ALPHAARG2:
			break;
		default:
			printf("Unhandled SetTextureStageState(%d, 0x%X, 0x%X)\n", Stage, Type, Value);
	}

/*	glActiveTexture(GL_TEXTURE0+0);
	glClientActiveTexture(GL_TEXTURE0+0);*/

	return S_OK;
}

HRESULT IDirect3DDevice9::SetTexture(DWORD Sampler, IDirect3DTexture9 *pTexture)
{
	if(Sampler) {
		glActiveTexture(GL_TEXTURE0+Sampler);
		glClientActiveTexture(GL_TEXTURE0+Sampler);
	}

	pTexture->Bind();

#ifdef SCR_SHARP_PIXEL
	if(!Sampler)
		mTexture0 = pTexture;		// the sharp-pixel shader needs its dimensions
#endif

	if(Sampler) {
		glActiveTexture(GL_TEXTURE0);
		glClientActiveTexture(GL_TEXTURE0);
	}
	return S_OK;
}

HRESULT IDirect3DDevice9::Clear(DWORD Count, const D3DRECT *pRects,DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil)
{
	GLbitfield clearval = 0;
	if(Flags&D3DCLEAR_STENCIL) {
		glClearStencil(Stencil);
		clearval |= GL_STENCIL_BUFFER_BIT;
	}
	if(Flags&D3DCLEAR_ZBUFFER) {
		glClearDepth(Z);
		clearval |= GL_DEPTH_BUFFER_BIT;
	}
	if(Flags&D3DCLEAR_TARGET) {
		float r,g,b,a;
		// D3DCOLOR is ARGB: 0xaarrggbb.
		b = ((Color>>0 )&0xff)/255.0f;
		g = ((Color>>8 )&0xff)/255.0f;
		r = ((Color>>16)&0xff)/255.0f;
		a = ((Color>>24)&0xff)/255.0f;
		glClearColor(r, g, b, a);
		clearval |= GL_COLOR_BUFFER_BIT;
	}
	if(clearval)
		glClear(clearval);
	return S_OK;
}

HRESULT IDirect3DDevice9::CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF, D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE *pSharedHandle) {
	*ppVertexBuffer = new IDirect3DVertexBuffer9(Length, FVF);

	return S_OK;
}

HRESULT IDirect3DDevice9::SetStreamSource(UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride) {
	buffer[StreamNumber] = pStreamData;
	offset[StreamNumber] = OffsetInBytes;
	stride[StreamNumber] = Stride;
	return S_OK;
}

HRESULT IDirect3DDevice9::SetFVF(DWORD FVF) {
	fvf = FVF;
	return S_OK;
}

IDirect3DVertexBuffer9::IDirect3DVertexBuffer9(uint32_t size, uint32_t fvf) {
	buffer.fvf = fvf;
	buffer.buffer = malloc(size);
}

IDirect3DVertexBuffer9::~IDirect3DVertexBuffer9() {
	Release();
}

HRESULT IDirect3DVertexBuffer9::Lock(UINT OffsetToLock, UINT SizeToLock, void **ppbData,DWORD Flags) {
	// very basic
	*ppbData = (void*)((char*)buffer.buffer + OffsetToLock);
	return S_OK;
}

HRESULT IDirect3DVertexBuffer9::Unlock() {
	// (I told you, very basic)
	return S_OK;
}

HRESULT IDirect3DVertexBuffer9::Release() {
	free(buffer.buffer);
	return S_OK;
}


// The Amiga original's built-in 8x8 bitmap font ("font7" in the 68k source, chars 32..126).
// Glyphs live in the top 7 bits of each byte; the print routine advances 7 pixels per column.
// The three glyphs the game patches at startup (decimal point, minus, underscore) are
// already applied here.
static const unsigned char font7[95][8] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },	// 32 space
	{ 0x95, 0x95, 0x95, 0x95, 0xAA, 0xEA, 0xEA, 0xEA },
	{ 0x15, 0x15, 0x15, 0x15, 0x15, 0x6A, 0x6A, 0x6A },
	{ 0x75, 0xC3, 0x00, 0x00, 0x00, 0x00, 0x80, 0x80 },
	{ 0x40, 0x40, 0xC0, 0x00, 0x00, 0x80, 0x80, 0x80 },
	{ 0x55, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA },
	{ 0x55, 0x55, 0x55, 0x55, 0x55, 0xAA, 0xAA, 0xAA },
	{ 0xBD, 0xFF, 0xC3, 0xC0, 0xC3, 0xF3, 0xBF, 0xBF },
	{ 0x00, 0x00, 0xC0, 0xC0, 0xC0, 0xC0, 0x40, 0x40 },
	{ 0xFF, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80 },
	{ 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0xFF },
	{ 0x08, 0x08, 0x08, 0x7F, 0x08, 0x08, 0x08, 0x00 },
	{ 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0xFF },
	{ 0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00 },	// 45 '-' (patched)
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00 },	// 46 '.' (patched)
	{ 0x00, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x00 },
	{ 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00 },	// 48 '0'
	{ 0x00, 0x10, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00 },
	{ 0x00, 0x3C, 0x42, 0x0C, 0x30, 0x40, 0x7E, 0x00 },
	{ 0x00, 0x7E, 0x04, 0x0C, 0x02, 0x42, 0x3C, 0x00 },
	{ 0x00, 0x04, 0x0C, 0x14, 0x24, 0x7E, 0x04, 0x00 },
	{ 0x00, 0x7E, 0x40, 0x7C, 0x02, 0x02, 0x7C, 0x00 },
	{ 0x00, 0x3C, 0x40, 0x7C, 0x42, 0x42, 0x3C, 0x00 },
	{ 0x00, 0x7E, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00 },
	{ 0x00, 0x3C, 0x42, 0x3C, 0x42, 0x42, 0x3C, 0x00 },
	{ 0x00, 0x3C, 0x42, 0x3C, 0x04, 0x08, 0x10, 0x00 },
	{ 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x00, 0x00 },
	{ 0x00, 0x00, 0x10, 0x00, 0x00, 0x10, 0x20, 0x00 },
	{ 0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00 },
	{ 0x00, 0x00, 0x7E, 0x00, 0x7E, 0x00, 0x00, 0x00 },
	{ 0x30, 0x18, 0x0C, 0x06, 0x0C, 0x18, 0x30, 0x00 },
	{ 0x00, 0x38, 0x44, 0x04, 0x08, 0x10, 0x00, 0x10 },
	{ 0x3C, 0x66, 0x6E, 0x6A, 0x6E, 0x60, 0x3C, 0x00 },
	{ 0x00, 0x3C, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x00 },	// 65 'A'
	{ 0x00, 0x78, 0x44, 0x7C, 0x42, 0x42, 0x7C, 0x00 },
	{ 0x00, 0x3C, 0x42, 0x40, 0x40, 0x42, 0x3C, 0x00 },
	{ 0x00, 0x7C, 0x42, 0x42, 0x42, 0x42, 0x7C, 0x00 },
	{ 0x00, 0x7E, 0x40, 0x78, 0x40, 0x40, 0x7E, 0x00 },
	{ 0x00, 0x7E, 0x40, 0x78, 0x40, 0x40, 0x40, 0x00 },
	{ 0x00, 0x3C, 0x42, 0x40, 0x4E, 0x42, 0x3E, 0x00 },
	{ 0x00, 0x42, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00 },
	{ 0x00, 0x38, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 },
	{ 0x00, 0x04, 0x04, 0x04, 0x04, 0x44, 0x38, 0x00 },
	{ 0x00, 0x44, 0x48, 0x70, 0x48, 0x44, 0x42, 0x00 },
	{ 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3E, 0x00 },
	{ 0x00, 0x42, 0x66, 0x5A, 0x42, 0x42, 0x42, 0x00 },
	{ 0x00, 0x42, 0x62, 0x52, 0x4A, 0x46, 0x42, 0x00 },
	{ 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00 },
	{ 0x00, 0x7C, 0x42, 0x7C, 0x40, 0x40, 0x40, 0x00 },
	{ 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x06 },
	{ 0x00, 0x7C, 0x42, 0x7C, 0x48, 0x44, 0x42, 0x00 },
	{ 0x00, 0x3E, 0x40, 0x3C, 0x02, 0x02, 0x7C, 0x00 },
	{ 0x00, 0x7C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x00 },
	{ 0x00, 0x42, 0x42, 0x42, 0x42, 0x42, 0x3E, 0x00 },
	{ 0x00, 0x42, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00 },
	{ 0x00, 0x42, 0x42, 0x42, 0x5A, 0x66, 0x42, 0x00 },
	{ 0x00, 0x42, 0x24, 0x18, 0x18, 0x24, 0x42, 0x00 },
	{ 0x00, 0x44, 0x44, 0x28, 0x10, 0x10, 0x10, 0x00 },
	{ 0x00, 0x7E, 0x04, 0x08, 0x10, 0x20, 0x7E, 0x00 },	// 90 'Z'
	{ 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },
	{ 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },
	{ 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x01 },
	{ 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x80 },
	{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7E, 0x00 },	// 95 '_' (patched)
	{ 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF },
	{ 0x00, 0x00, 0x3C, 0x02, 0x3E, 0x42, 0x3E, 0x00 },	// 97 'a'
	{ 0x00, 0x40, 0x7C, 0x42, 0x42, 0x42, 0x7C, 0x00 },
	{ 0x00, 0x00, 0x3E, 0x40, 0x40, 0x40, 0x3E, 0x00 },
	{ 0x00, 0x02, 0x3E, 0x42, 0x42, 0x42, 0x3E, 0x00 },
	{ 0x00, 0x00, 0x3C, 0x42, 0x7E, 0x40, 0x3C, 0x00 },
	{ 0x00, 0x1C, 0x22, 0x20, 0x78, 0x20, 0x20, 0x00 },
	{ 0x00, 0x00, 0x3E, 0x42, 0x42, 0x3E, 0x02, 0x3C },
	{ 0x00, 0x40, 0x40, 0x7C, 0x42, 0x42, 0x42, 0x00 },
	{ 0x10, 0x00, 0x30, 0x10, 0x10, 0x10, 0x38, 0x00 },
	{ 0x00, 0x08, 0x00, 0x08, 0x08, 0x08, 0x48, 0x30 },
	{ 0x00, 0x20, 0x20, 0x24, 0x38, 0x24, 0x22, 0x00 },
	{ 0x00, 0x30, 0x10, 0x10, 0x10, 0x10, 0x38, 0x00 },
	{ 0x00, 0x00, 0x24, 0x5A, 0x5A, 0x42, 0x42, 0x00 },
	{ 0x00, 0x00, 0x7C, 0x42, 0x42, 0x42, 0x42, 0x00 },
	{ 0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x3C, 0x00 },
	{ 0x00, 0x00, 0x7C, 0x42, 0x42, 0x7C, 0x40, 0x40 },
	{ 0x00, 0x00, 0x3E, 0x42, 0x42, 0x3E, 0x02, 0x02 },
	{ 0x00, 0x00, 0x5C, 0x62, 0x40, 0x40, 0x40, 0x00 },
	{ 0x00, 0x00, 0x3E, 0x60, 0x3C, 0x06, 0x7C, 0x00 },
	{ 0x00, 0x20, 0x7C, 0x20, 0x20, 0x24, 0x18, 0x00 },
	{ 0x00, 0x00, 0x42, 0x42, 0x42, 0x42, 0x3E, 0x00 },
	{ 0x00, 0x00, 0x42, 0x42, 0x42, 0x24, 0x18, 0x00 },
	{ 0x00, 0x00, 0x42, 0x42, 0x5A, 0x5A, 0x24, 0x00 },
	{ 0x00, 0x00, 0x42, 0x24, 0x18, 0x24, 0x42, 0x00 },
	{ 0x00, 0x00, 0x42, 0x42, 0x42, 0x3E, 0x02, 0x3C },
	{ 0x00, 0x00, 0x7E, 0x04, 0x18, 0x20, 0x7E, 0x00 },	// 122 'z'
	{ 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x81 },
	{ 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81, 0x81 },
	{ 0x81, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81 },
	{ 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF },	// 126
};

#define FONT7_CELL		8	// glyph cell, pixels
#define FONT7_ADVANCE	7	// print.character steps column*8 - column

CDXUTTextHelper::CDXUTTextHelper(TTF_Font* font, GLuint sprite, int size) :
	m_sprite(sprite), m_size(size), m_posx(0), m_posy(0)
{
	// set colors
	m_forecol[0] = m_forecol[1] = m_forecol[2] = m_forecol[3] = 1.0f;

	// Integer magnification horizontally, then the Amiga's non-square pixel vertically:
	// all the 320x200 art is drawn at x2 across and x2.4 down, so the font has to match or
	// it sits squat next to the dashboard it shares a box with.
	m_scale = (size + FONT7_CELL/2) / FONT7_CELL;
	if (m_scale < 1) m_scale = 1;
	m_scaley = m_scale * (2.4f / 2.0f);
	m_fontsize = FONT7_CELL;
	m_size = static_cast<int>(FONT7_CELL * m_scaley + 0.5f);

	// Build a 16x16 grid of 8x8 cells (128x128) holding chars 0..255; only 32..126 exist.
	m_sizew = m_sizeh = 16 * FONT7_CELL;
	unsigned char *tex = (unsigned char*)malloc(m_sizew * m_sizeh * 4);
	memset(tex, 0, m_sizew * m_sizeh * 4);
	for (int ch = 32; ch <= 126; ch++)
	{
		const unsigned char *glyph = font7[ch - 32];
		int ox = (ch % 16) * FONT7_CELL, oy = (ch / 16) * FONT7_CELL;
		for (int row = 0; row < 8; row++)
		{
			// bit 0 is never drawn by the original (7-bit mask), so discard it
			unsigned char bits = glyph[row] & 0xFE;
			for (int col = 0; col < 8; col++)
			{
				if (!((bits >> (7 - col)) & 1)) continue;
				unsigned char *p = tex + (((oy + row) * m_sizew) + ox + col) * 4;
				p[0] = p[1] = p[2] = p[3] = 255;
			}
		}
	}

	glGenTextures(1, &m_texture);
	glBindTexture(GL_TEXTURE_2D, m_texture);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_sizew, m_sizeh, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);
	free(tex);
	glBindTexture(GL_TEXTURE_2D, 0);

	m_inv = 1.0f / (float)m_sizew;
	for (int i = 0; i < 256; i++)
		m_as[i] = FONT7_ADVANCE * m_scale;
}

CDXUTTextHelper::~CDXUTTextHelper()
{
	glDeleteTextures(1, &m_texture);
}

void CDXUTTextHelper::SetInsertionPos(int x, int y)
{
	m_posx = x;
	m_posy = y;
}

void CDXUTTextHelper::DrawTextLine(const wchar_t* line)
{
	// Draw it
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glBindTexture(GL_TEXTURE_2D, m_texture);
	glColor4fv(m_forecol);
	glBegin(GL_QUADS);
	int i=0;
	wchar_t wch;
	float posx = m_posx;
	float cellw = (float)(m_fontsize * m_scale);
	float cellh = m_fontsize * m_scaley;
	while((wch=line[i]))
	{
		unsigned int ch = (wch >= 32 && wch <= 126) ? (unsigned int)wch : 32;
		float u0 = (ch%16)*m_fontsize*m_inv, v0 = (ch/16)*m_fontsize*m_inv;
		float u1 = u0 + m_fontsize*m_inv, v1 = v0 + m_fontsize*m_inv;
		glTexCoord2f(u0,v0); glVertex2f(posx, m_posy);
		glTexCoord2f(u1,v0); glVertex2f(posx+cellw, m_posy);
		glTexCoord2f(u1,v1); glVertex2f(posx+cellw, m_posy + cellh);
		glTexCoord2f(u0,v1); glVertex2f(posx, m_posy + cellh);
		posx += m_as[ch];
		i++;
	}
	glEnd();
	glBindTexture(GL_TEXTURE_2D, 0);
	m_posy += m_size;

	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);
	glEnable(GL_DEPTH_TEST);

}

void CDXUTTextHelper::DrawFormattedTextLine(const wchar_t* line, ...)
{
	wchar_t buff[1000];
	va_list args;
  	va_start (args, line);
	vswprintf(buff, 1000, line, args);
	DrawTextLine(buff);
	va_end (args);
}

void CDXUTTextHelper::SetForegroundColor(D3DXCOLOR clr)
{
	m_forecol[0] = clr.r;
	m_forecol[1] = clr.g;
	m_forecol[2] = clr.b;
	m_forecol[3] = clr.a;
}

static IDirect3DDevice9* device = NULL;
IDirect3DDevice9 *DXUTGetD3DDevice()
{
	if (!device)
		device = new IDirect3DDevice9();
	return device;
}

static D3DSURFACE_DESC d3dsurface_desc = {0}; 
const D3DSURFACE_DESC * DXUTGetBackBufferSurfaceDesc()
{
	/*int vp[4];
	glGetIntegerv(GL_VIEWPORT, vp);
	d3dsurface_desc.Width = vp[2];
	d3dsurface_desc.Height = vp[3];*/
	d3dsurface_desc.Width = wideScreen?800:640;
	d3dsurface_desc.Height = 480;
	return &d3dsurface_desc;
}

DOUBLE DXUTGetTime()
{
	return ((DOUBLE)SDL_GetTicks())/1000.0;
}

void DXUTReset3DEnvironment()
{
	// NOTHING?
}

#endif
