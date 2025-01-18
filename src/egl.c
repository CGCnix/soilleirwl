#include "soilleirwl/interfaces/swl_output.h"
#include <soilleirwl/renderer.h>
#include <soilleirwl/logger.h>
#include <soilleirwl/private/drm_output.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglplatform.h>
#include <stdint.h>
#include <stdio.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <wayland-util.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

#include <sys/mman.h>

typedef struct swl_egl_functions {
	PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC EGLImageTargetRenderbufferStorageOES;
	PFNEGLCREATEIMAGEKHRPROC EGLCreateImageKHR;
	PFNEGLGETPLATFORMDISPLAYEXTPROC EGLGetPlatformDisplayEXT;
	PFNEGLQUERYDEVICESTRINGEXTPROC eglQueryDeviceStringEXT;
	PFNEGLQUERYDEVICESEXTPROC eglQueryDevicesEXT;
} swl_egl_functions_t;

typedef struct swl_egl_render_target {
	swl_output_t *output;

	EGLImage images[2]; /*Front and back*/
	GLuint rbo[2], fbo[2];
	
	struct wl_list link;
} swl_egl_renderer_target_t;

typedef struct swl_egl_renderer {
	swl_renderer_t common;
	EGLDisplay display;
	EGLDeviceEXT egldevice;

	EGLContext ctx;
	int drmfd;
	swl_egl_functions_t funcs;
	
	swl_egl_renderer_target_t *current;
	GLuint texture_shader; 
	struct wl_list targets;
} swl_egl_renderer_t;

int swl_egl_ext_present(const char *extensions, const char *desired) {
	return strstr(extensions, desired) ? 1 : 0;
}

int swl_gl_ext_present(const GLubyte *extensions, const char *desired) {
	return strstr((const char*)extensions, desired) ? 1 : 0;
}


uint32_t swl_egl_msgtype_to_log_level(EGLint type) {
	switch(type) {
		case EGL_DEBUG_MSG_ERROR_KHR:
		case EGL_DEBUG_MSG_CRITICAL_KHR:
			return SWL_LOG_ERROR;
		case EGL_DEBUG_MSG_WARN_KHR: return SWL_LOG_WARN;
		case EGL_DEBUG_MSG_INFO_KHR: return SWL_LOG_INFO;
		default: return SWL_LOG_INFO;
	}
}

void swl_egl_debug_log(EGLenum error, const char *cmd, EGLint msg_type,
		EGLLabelKHR thread, EGLLabelKHR object, const char *message) {
	uint32_t level = swl_egl_msgtype_to_log_level(msg_type);

	swl_log(level, __LINE__, __FILE__, "%d(%s) %s\n", error, cmd, message);
}

uint32_t swl_gl_severity_to_log_level(GLenum severity) {
	switch(severity) {
		case GL_DEBUG_SEVERITY_HIGH_KHR: return SWL_LOG_ERROR;
		case GL_DEBUG_SEVERITY_MEDIUM_KHR: return SWL_LOG_WARN;
		case GL_DEBUG_SEVERITY_LOW_KHR: return SWL_LOG_DEBUG;
		case GL_DEBUG_SEVERITY_NOTIFICATION_KHR: return SWL_LOG_INFO;
		default: return SWL_LOG_INFO;
	}
}

void swl_gl_debug_callback(GLenum source, GLenum type, GLuint id,
		GLenum severity, GLsizei length, const GLchar *message, const void *user_data) {
	uint32_t level = swl_gl_severity_to_log_level(severity);
	swl_log(level, __LINE__, __FILE__, "%d %d %d %s\n", source, type, id, message);
}

GLuint swl_egl_compile_shader(GLenum type, const char *src) {
	GLuint shader;
	GLint compiled, len;
  char *info;

	shader = glCreateShader(type);
	if(shader == 0) {
		 return 0;
	}
	glShaderSource(shader, 1, &src, NULL);
	glCompileShader(shader);
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if(!compiled) {
		len = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		if(len > 1) {
			info = calloc(sizeof(char), len);
			glGetShaderInfoLog(shader, len, NULL, info);
			swl_error("Shader error: %s\n", info);
			free(info);
		}

		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

void swl_egl_create_shader(swl_egl_renderer_t *renderer) {
	char *vert_shader = 
		"attribute vec4 vert_pos;\n"
		"attribute vec2 texcoord;\n"
		"varying vec2 f_texcoord;\n"
		"void main()\n"
		"{\n"
		"	gl_Position = vert_pos;\n"
		"	f_texcoord = vec2(texcoord.x, texcoord.y);\n"
		"}\n";

	char *frag_shader = 
		"precision mediump float;\n"
		"uniform sampler2D client_texture;\n"
		"varying vec2 f_texcoord;\n"
		"void main()\n"
		"{\n"
		"	gl_FragColor = texture2D(client_texture, f_texcoord);\n"
		"}\n";

	GLuint vertexShader;
	GLuint fragmentShader;
	GLint linked;

	vertexShader = swl_egl_compile_shader(GL_VERTEX_SHADER, vert_shader);
	fragmentShader = swl_egl_compile_shader(GL_FRAGMENT_SHADER, frag_shader);

	renderer->texture_shader = glCreateProgram();

	if (renderer->texture_shader == 0)
		return;

	glAttachShader(renderer->texture_shader, vertexShader);

	glAttachShader(renderer->texture_shader, fragmentShader);
	glLinkProgram(renderer->texture_shader);
	glGetProgramiv(renderer->texture_shader, GL_LINK_STATUS, &linked);
	if(!linked) {
		GLint infoLen = 0;
		glGetProgramiv(renderer->texture_shader, GL_INFO_LOG_LENGTH, &infoLen);

		if(infoLen > 1) {
			char* infoLog = malloc(sizeof(char) * infoLen);

			glGetProgramInfoLog(renderer->texture_shader, infoLen, NULL, infoLog);
			swl_error("GL shader linking failed: %s\n", infoLog) 
			free(infoLog);
		}

		glDeleteProgram (renderer->texture_shader);
		return;
	}
	glDeleteShader(fragmentShader);
	glDeleteShader(vertexShader);
	return;
}

int swl_egl_check_client_ext() {
	const char *extension = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
	if(extension == NULL) {
		swl_debug("No EGL Client Extensions present\n");
		return 1;
	}

	swl_info("EGL Client Extensions: %s\n", extension);
	/*We should really do some fallback for now this works*/
	if(!swl_egl_ext_present(extension, "EGL_KHR_debug")) {
		swl_warn("Error %s not supported\n", "EGL_KHR_debug");
		return 1;
	}
	if(!swl_egl_ext_present(extension, "EGL_EXT_device_enumeration")) {
		swl_warn("Error %s not supported\n", "EGL_EXT_device_enumeration");
		return 1;
	}
	if(!swl_egl_ext_present(extension, "EGL_EXT_platform_base")) {
		swl_warn("Error %s not supported\n", "EGL_EXT_platform_base");
		return 1;
	}
	if(!swl_egl_ext_present(extension, "EGL_EXT_client_extensions")) {
		swl_warn("Error %s not supported\n", "EGL_EXT_client_extensions");
		return 1;
	}
	if(!swl_egl_ext_present(extension, "EGL_EXT_device_query")) {
		swl_warn("Error %s not supported\n", "EGL_EXT_device_query");
		return 1;
	}
	if(!swl_egl_ext_present(extension, "EGL_EXT_platform_device")) {
		swl_warn("Error %s not supported\n", "EGL_EXT_platform_device");
		return 1;
	}
	
	return 0;
}

int swl_egl_check_display_ext(EGLDisplay *display) {
	const char *extension = eglQueryString(display, EGL_EXTENSIONS);
	if(extension == NULL) {
		swl_debug("No EGL Display Extensions present\n");
		return 1;
	}

	swl_info("EGL Display Extensions: %s\n", extension);

	if(!swl_egl_ext_present(extension, "EGL_EXT_image_dma_buf_import")) {
		swl_error("Error %s not supported\n", "EGL_EXT_image_dma_buf_import");
		return 1;
	}
	if(!swl_egl_ext_present(extension, "EGL_KHR_image_base")) {
		swl_error("Error %s not supported\n", "EGL_KHR_image_base");
		return 1;
	}
	
	return 0;
}

int swl_gl_check_ext() {
	const GLubyte *extension = glGetString(GL_EXTENSIONS);// eglQueryString(display, EGL_EXTENSIONS);
	if(extension == NULL) {
		swl_debug("No GLes Extensions present\n");
		return 1;
	}

	swl_info("GLes Extensions: %s\n", extension);

	if(!swl_gl_ext_present(extension, "GL_OES_EGL_image")) {
		swl_error("Error %s not supported\n", "glEGLImageTargetRenderbufferStorageOES");
		return 1;
	}
	if(!swl_gl_ext_present(extension, "GL_KHR_debug")) {
		swl_error("Error %s not supported\n", "GL_KHR_debug");
		return 1;
	}
	
	return 0;
}

swl_egl_renderer_target_t *swl_egl_get_target(swl_output_t *output, struct wl_list *list) {
	swl_egl_renderer_target_t *target;
	wl_list_for_each(target, list, link) {
		if(output == target->output) {
			return target;
		}
	}
	return NULL;
}

EGLImage swl_egl_import_dma_buf(swl_egl_renderer_t *egl, int dma_buf, EGLint height, EGLint width, EGLint stride, EGLint offset) {
	EGLint attr[15];
	attr[0] = EGL_WIDTH;
	attr[1] = width;
	attr[2] = EGL_HEIGHT;
	attr[3] = height;
	attr[4] = EGL_LINUX_DRM_FOURCC_EXT;
	attr[5] = DRM_FORMAT_XRGB8888;

	attr[6] = EGL_DMA_BUF_PLANE0_FD_EXT;
	attr[7] = dma_buf;
	attr[8] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
	attr[9] = offset;
	attr[10] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
	attr[11] = stride;
	attr[12] = EGL_IMAGE_PRESERVED_KHR;
	attr[13] = EGL_TRUE;

	attr[14] = EGL_NONE;
	return egl->funcs.EGLCreateImageKHR(egl->display, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attr);
}

void swl_egl_output_attach(swl_renderer_t *render, swl_output_t *output) {
	swl_egl_renderer_t *egl;
	swl_egl_renderer_target_t *target;

	egl = (swl_egl_renderer_t*)render;
	eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
			egl->ctx);
	if(egl->current) {
		glDeleteFramebuffers(2, egl->current->fbo);
		glDeleteRenderbuffers(2, egl->current->rbo);
		eglDestroyImage(egl->display, egl->current->images[0]);
		eglDestroyImage(egl->display, egl->current->images[1]);
		free(egl->current);
		egl->current = NULL;
	}

	/*Create it's render objects now then*/
	target = calloc(1, sizeof(swl_egl_renderer_target_t));
	int dmabuf;
	target->output = output;
	for(uint32_t buf = 0; buf < 2; ++buf) {
		drmPrimeHandleToFD(output->buffer[buf]->render, output->buffer[buf]->handle, DRM_CLOEXEC, &dmabuf);
	target->images[buf] = swl_egl_import_dma_buf(egl, dmabuf, output->buffer[buf]->height,
		output->buffer[buf]->width, output->buffer[buf]->pitch, 0);
		close(dmabuf);

		glGenRenderbuffers(1, &target->rbo[buf]);
		glBindRenderbuffer(GL_RENDERBUFFER, target->rbo[buf]);
		
		egl->funcs.EGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER, target->images[buf]);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
		glGenFramebuffers(1, &target->fbo[buf]);
		glBindFramebuffer(GL_FRAMEBUFFER, target->fbo[buf]);
		glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
		GL_RENDERBUFFER, target->rbo[buf]);
		glFlush();
		glFinish();
		GLenum fb_status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		if(fb_status != GL_FRAMEBUFFER_COMPLETE) {
			swl_error("Framebuffer %p %d error egl %d\n", target->images[buf], buf, fb_status);
			exit(1);
		}
	}

	/*Make this target the current target*/ 
	egl->current = target;
}

void swl_egl_begin(swl_renderer_t *renderer) {
	swl_egl_renderer_t *egl = (swl_egl_renderer_t*)renderer;
	uint32_t front = egl->current->output->front_buffer;
	eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE,
			egl->ctx);
	glBindFramebuffer(GL_FRAMEBUFFER, egl->current->fbo[front]);

	glViewport(0, 0, egl->current->output->buffer[front]->width,
			egl->current->output->buffer[front]->height);

	glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

void swl_egl_clear(swl_renderer_t *renderer, float r, float g, float b, float a) {
	glClearColor(r, g, b, a);
	glClear(GL_COLOR_BUFFER_BIT);
}

void swl_egl_end(swl_renderer_t *renderer) {
	/* Flush whatever is being rendered to ensure
	 * it is all rendered especially as if some software/other
	 * rendering is used this could cause Async problem if not flushed
	 */
	glFlush();
	glFinish();
}

typedef struct swl_egl_texture {
	GLuint id;
	int32_t width, height;
} swl_egl_texture_t;

GLenum external_format_to_gl(uint32_t format) {
	switch (format) {
		case WL_SHM_FORMAT_XRGB8888:
		case WL_SHM_FORMAT_ARGB8888:
		case DRM_FORMAT_XRGB8888:
		case DRM_FORMAT_ARGB8888:
			return GL_BGRA_EXT;
		case DRM_FORMAT_BGRA8888:
		case DRM_FORMAT_BGRX8888:
			return GL_RGBA;
		default:
			swl_debug("Unknown Format Assuming GL_BGRA_EXT\n");
			return GL_BGRA_EXT;
	}
}

swl_texture_t *swl_egl_create_texture(swl_renderer_t *render, uint32_t width, 
		uint32_t height, uint32_t format, void *data) {
	swl_egl_renderer_t *egl = (swl_egl_renderer_t*)render;
	swl_egl_texture_t *texture = calloc(1, sizeof(swl_egl_texture_t));
	GLenum glform = external_format_to_gl(format);

	texture->height = height;
	texture->width = width;
	eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl->ctx);		


	glGenTextures(1, &texture->id);
	glBindTexture(GL_TEXTURE_2D, texture->id);

	glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, width);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, glform, GL_UNSIGNED_BYTE, data);

		glPixelStorei(GL_UNPACK_ROW_LENGTH_EXT, 0);
	glBindTexture(GL_TEXTURE_2D, 0);
	return (swl_texture_t*)texture;
}

GLfloat normalize(GLuint min, GLuint max, GLuint val) {
	return 2.0f * ((GLfloat)(val - min) / (max - min)) - 1.0f;
}

void swl_egl_draw_texture(swl_renderer_t *render, swl_texture_t *texture_in, int32_t x, int32_t y) {
	swl_egl_renderer_t *egl = (swl_egl_renderer_t*)render;
	swl_egl_texture_t *texture = (swl_egl_texture_t*)texture_in;
	if (!texture || !texture->id) {
		return;
	}

	uint32_t height = egl->current->output->mode.height;
	uint32_t width = egl->current->output->mode.width;

	glEnable(GL_BLEND);	
	GLfloat verts[] = {
		//X|Y
		/*TODO: use the values from glViewport*/
		normalize(0, width, x + texture->width), normalize(0, height, y),
		normalize(0, width, x), normalize(0, height, y),
		normalize(0, width, x + texture->width), normalize(0, height, y + texture->height),
		normalize(0, width, x), normalize(0, height, y + texture->height),
		
	};

	GLfloat tex_coords[] = {
		1.0f, 0.0f,
		0.0f, 0.0f,
		1.0f, 1.0f,
		0.0f, 1.0f,
	};
	glUseProgram(egl->texture_shader);
	GLint pos_inx = glGetAttribLocation(egl->texture_shader, "vert_pos");
	glVertexAttribPointer(pos_inx, 2, GL_FLOAT, GL_FALSE, 0, verts);
	
	glEnableVertexAttribArray(0);

	glActiveTexture(GL_TEXTURE0);
	
	glBindTexture(GL_TEXTURE_2D, texture->id);
	GLint uv_inx = glGetAttribLocation(egl->texture_shader, "texcoord");
	glVertexAttribPointer(uv_inx, 2, GL_FLOAT, GL_FALSE, 0, tex_coords);
	glEnableVertexAttribArray(uv_inx);

	GLint tex_loc = glGetUniformLocation(egl->texture_shader, "client_texture" );

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);	
	glUniform1i(tex_loc, 0); // 0 == texture unit 0
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	glFlush();
	glFinish();
}

void swl_egl_destroy_texture(swl_renderer_t *render, swl_texture_t *texture) {
	swl_egl_texture_t *egl_text = (void*) texture;
	glDeleteTextures(1, &egl_text->id);
	free(egl_text);
}

void swl_egl_renderer_destroy(swl_renderer_t *renderer) {
	swl_egl_renderer_t *egl = (swl_egl_renderer_t*)renderer;

	glDeleteProgram(egl->texture_shader);

	if(egl->current) {
		glDeleteFramebuffers(2, egl->current->fbo);
		glDeleteRenderbuffers(2, egl->current->rbo);
		eglDestroyImage(egl->display, egl->current->images[0]);
		eglDestroyImage(egl->display, egl->current->images[1]);
		free(egl->current);
		egl->current = NULL;
	}

	eglDestroyContext(egl->display, egl->current);
	eglTerminate(egl->display);
	free(egl);
}

swl_renderer_t *swl_egl_renderer_create_by_fd(int drm_fd) {
	swl_egl_renderer_t *egl;
	drmDevicePtr drm_dev;
	EGLDeviceEXT *devices, preffered;
	EGLint dev_count, dev, major, minor, ctx_attributes[3];
	const char *egl_dev_string;
	EGLAttrib debug_attr[] = { EGL_DEBUG_MSG_INFO_KHR, EGL_TRUE, EGL_NONE };

	/*exit if extensions needed don't exist*/
	if(swl_egl_check_client_ext()) {
		swl_error("Required Client extensions missing\n");
		return NULL;
	}
	
	PFNEGLDEBUGMESSAGECONTROLKHRPROC eglDebugMessageControl = (PFNEGLDEBUGMESSAGECONTROLKHRPROC)eglGetProcAddress("eglDebugMessageControlKHR");
	
	eglDebugMessageControl(swl_egl_debug_log, debug_attr);
	
	egl = calloc(1, sizeof(*egl));
	
	/*Setup function pointers*/
	egl->common.begin = swl_egl_begin;
	egl->common.attach_output = swl_egl_output_attach;
	egl->common.clear = swl_egl_clear;
	egl->common.end = swl_egl_end;	
	egl->common.create_texture = swl_egl_create_texture;
	egl->common.destroy_texture = swl_egl_destroy_texture;
	egl->common.draw_texture = swl_egl_draw_texture;
	egl->common.destroy = swl_egl_renderer_destroy;
	wl_list_init(&egl->targets);

	/*Get client extension Functions*/
	egl->funcs.eglQueryDevicesEXT = (PFNEGLQUERYDEVICESEXTPROC) eglGetProcAddress("eglQueryDevicesEXT");
	egl->funcs.eglQueryDeviceStringEXT = (PFNEGLQUERYDEVICESTRINGEXTPROC) eglGetProcAddress("eglQueryDeviceStringEXT");
	egl->funcs.EGLGetPlatformDisplayEXT = (PFNEGLGETPLATFORMDISPLAYEXTPROC) eglGetProcAddress("eglGetPlatformDisplayEXT");

	drmGetDevice(drm_fd, &drm_dev);
	dev_count = 0;
	if(egl->funcs.eglQueryDevicesEXT(0, NULL, &dev_count) == EGL_FALSE) {
		swl_error("Query devices call 1 failed\n");
		return NULL;
	}

	devices = calloc(dev_count, sizeof(EGLDeviceEXT));
	if(egl->funcs.eglQueryDevicesEXT(dev_count, devices, &dev_count) == EGL_FALSE) {
		swl_error("Query devices call 2 failed\n");
		return NULL;
	}
		
	for(dev = 0; dev < dev_count; dev++) {
		egl_dev_string = egl->funcs.eglQueryDeviceStringEXT(devices[dev], EGL_DRM_DEVICE_FILE_EXT);

		swl_debug("DRM FILE: %s\n", egl_dev_string);
		if(strcmp(drm_dev->nodes[0], egl_dev_string) == 0) {
			preffered = devices[dev];
			break;
		}
	}
	egl->drmfd = drm_fd;
	egl->display = egl->funcs.EGLGetPlatformDisplayEXT(EGL_PLATFORM_DEVICE_EXT, preffered, NULL);
	eglInitialize(egl->display, &major, &minor);
	swl_debug("EGL Version: %d %d\n", major, minor);
	eglBindAPI(EGL_OPENGL_ES_API);
	
	ctx_attributes[0] = EGL_CONTEXT_CLIENT_VERSION;
	ctx_attributes[1] = 2;
	ctx_attributes[2] = EGL_NONE;

	/*Check for the Display Extensions*/
	if(swl_egl_check_display_ext(egl->display)) {
		swl_error("Required EGL display extensions missing\n");
		return NULL;
	}

	egl->ctx = eglCreateContext(egl->display, EGL_NO_CONFIG_KHR, EGL_NO_CONTEXT, ctx_attributes);
	eglMakeCurrent(egl->display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl->ctx);	
	
	swl_debug("GLES2 Version: %s\n", glGetString(GL_VERSION));
	swl_debug("GLES2 Vendor: %s\n", glGetString(GL_VENDOR));
	swl_debug("GLES2 Renderer: %s\n", glGetString(GL_RENDERER));
	
	/*Check for the GLES Extensions*/
	if(swl_gl_check_ext()) {
		swl_error("Required GLes extensions missing\n");
		return NULL;
	}

	const char *extension = eglQueryString(egl->display, EGL_EXTENSIONS);
	egl->funcs.EGLCreateImageKHR = (void*) eglGetProcAddress("eglCreateImageKHR");


	egl->funcs.EGLImageTargetRenderbufferStorageOES = (void *) eglGetProcAddress("glEGLImageTargetRenderbufferStorageOES");
	PFNGLDEBUGMESSAGECALLBACKKHRPROC debug_callback = (PFNGLDEBUGMESSAGECALLBACKKHRPROC) eglGetProcAddress("glDebugMessageCallbackKHR");
	PFNGLDEBUGMESSAGECONTROLKHRPROC debug_message_control = (PFNGLDEBUGMESSAGECONTROLKHRPROC) eglGetProcAddress("glDebugMessageControlKHR");
	
	debug_callback(swl_gl_debug_callback, NULL);
	
	glEnable(GL_DEBUG_OUTPUT_KHR);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
	swl_egl_create_shader(egl);
	return (swl_renderer_t*)egl;
}
