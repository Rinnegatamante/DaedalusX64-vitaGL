#ifdef ENABLE_OPENGL

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#include <map>
#include <unordered_map>

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

#ifdef __MINGW32__
#define FOR_WINDOWS 1
#else
#define FOR_WINDOWS 0
#endif

#include "fast/backends/gfx_opengl.h"
#ifdef __vita__
#include "fast/vita_compat.h"
#endif

#ifdef __vita__
#include <psp2/gxm.h>
extern "C" {
    void vglBufferData(GLenum target, const GLvoid *data);
};
#define SHADER_MAGIC (1)
#endif

#ifdef __vita__
extern "C" bool DaedalusFast3D_ShouldSuppressFramebufferClear(bool color, bool depth);
extern "C" uint32_t DaedalusFast3D_GetFramebufferOverride(int fbId);
#endif

namespace Fast {
int GfxRenderingAPIOGL::GetMaxTextureSize() {
    return 1024;
}

const char* GfxRenderingAPIOGL::GetName() {
    return "OpenGL";
}

bool GfxRenderingAPIOGL::GetClipParameters() {
    return mFrameBuffers[mCurrentFrameBuffer].invertY;
}

static void VertexArraySetAttribs(ShaderProgram* prg) {
    size_t numFloats = prg->numFloats;
    size_t pos = 0;

    for (int i = 0; i < prg->numAttribs; i++) {
        if (prg->attribLocations[i] >= 0) {
            glEnableVertexAttribArray(prg->attribLocations[i]);
            glVertexAttribPointer(prg->attribLocations[i], prg->attribSizes[i], GL_FLOAT, GL_FALSE,
                                  numFloats * sizeof(float), (void*)(pos * sizeof(float)));
        }
        pos += prg->attribSizes[i];
    }
}

void GfxRenderingAPIOGL::SetUniforms(ShaderProgram* prg) const {
    glUniform1i(prg->frameCountLocation, mFrameCount);
    glUniform1f(prg->noiseScaleLocation, mCurrentNoiseScale);
}

void GfxRenderingAPIOGL::SetPerDrawUniforms() {
}

void GfxRenderingAPIOGL::UnloadShader(ShaderProgram* old_prg) {
    if (old_prg != nullptr && old_prg == mLastLoadedShader) {
        for (unsigned int i = 0; i < old_prg->numAttribs; i++) {
            if (old_prg->attribLocations[i] >= 0) {
                glDisableVertexAttribArray(old_prg->attribLocations[i]);
            }
        }
        mLastLoadedShader = nullptr;
    }
}

void GfxRenderingAPIOGL::LoadShader(ShaderProgram* new_prg) {
    // if (!new_prg) return;
    mCurrentShaderProgram = new_prg;
    if (new_prg != mLastLoadedShader) {
        glUseProgram(new_prg->openglProgramId);
        VertexArraySetAttribs(new_prg);
        mLastLoadedShader = new_prg;
    }
    SetUniforms(new_prg);
}

#define RAND_NOISE "((random(vec3(floor(gl_FragCoord.xy * noise_scale), float(frame_count))) + 1.0) / 2.0)"

static const char* shader_item_to_str(uint32_t item, bool with_alpha, bool only_alpha, bool inputs_have_alpha,
                                      bool first_cycle, bool hint_single_element) {
    if (!only_alpha) {
        switch (item) {
            case SHADER_0:
                return with_alpha ? "vec4(0.0, 0.0, 0.0, 0.0)" : "vec3(0.0, 0.0, 0.0)";
            case SHADER_1:
                return with_alpha ? "vec4(1.0, 1.0, 1.0, 1.0)" : "vec3(1.0, 1.0, 1.0)";
            case SHADER_INPUT_1:
                return with_alpha || !inputs_have_alpha ? "vInput1" : "vInput1.rgb";
            case SHADER_INPUT_2:
                return with_alpha || !inputs_have_alpha ? "vInput2" : "vInput2.rgb";
            case SHADER_INPUT_3:
                return with_alpha || !inputs_have_alpha ? "vInput3" : "vInput3.rgb";
            case SHADER_INPUT_4:
                return with_alpha || !inputs_have_alpha ? "vInput4" : "vInput4.rgb";
            case SHADER_TEXEL0:
                return first_cycle ? (with_alpha ? "texVal0" : "texVal0.rgb")
                                   : (with_alpha ? "texVal1" : "texVal1.rgb");
            case SHADER_TEXEL0A:
                return first_cycle
                           ? (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"))
                           : (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"));
            case SHADER_TEXEL1A:
                return first_cycle
                           ? (hint_single_element ? "texVal1.a"
                                                  : (with_alpha ? "vec4(texVal1.a, texVal1.a, texVal1.a, texVal1.a)"
                                                                : "vec3(texVal1.a, texVal1.a, texVal1.a)"))
                           : (hint_single_element ? "texVal0.a"
                                                  : (with_alpha ? "vec4(texVal0.a, texVal0.a, texVal0.a, texVal0.a)"
                                                                : "vec3(texVal0.a, texVal0.a, texVal0.a)"));
            case SHADER_TEXEL1:
                return first_cycle ? (with_alpha ? "texVal1" : "texVal1.rgb")
                                   : (with_alpha ? "texVal0" : "texVal0.rgb");
            case SHADER_COMBINED:
                return with_alpha ? "texel" : "texel.rgb";
            case SHADER_NOISE:
                return with_alpha ? "vec4(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")"
                                  : "vec3(" RAND_NOISE ", " RAND_NOISE ", " RAND_NOISE ")";
        }
    } else {
        switch (item) {
            case SHADER_0:
                return "0.0";
            case SHADER_1:
                return "1.0";
            case SHADER_INPUT_1:
                return "vInput1.a";
            case SHADER_INPUT_2:
                return "vInput2.a";
            case SHADER_INPUT_3:
                return "vInput3.a";
            case SHADER_INPUT_4:
                return "vInput4.a";
            case SHADER_TEXEL0:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL0A:
                return first_cycle ? "texVal0.a" : "texVal1.a";
            case SHADER_TEXEL1A:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_TEXEL1:
                return first_cycle ? "texVal1.a" : "texVal0.a";
            case SHADER_COMBINED:
                return "texel.a";
            case SHADER_NOISE:
                return RAND_NOISE;
        }
    }
    return "";
}

static void append_str(char* buf, size_t* len, const char* str) {
    while (*str != '\0') {
        buf[(*len)++] = *str++;
    }
}

static void append_line(char* buf, size_t* len, const char* str) {
    while (*str != '\0') {
        buf[(*len)++] = *str++;
    }
    buf[(*len)++] = '\n';
}

static void append_formula(char* buf, size_t* len, const int c[2][4],
                           bool do_single, bool do_multiply, bool do_mix,
                           bool with_alpha, bool only_alpha, bool opt_alpha, bool first_cycle) {
    if (do_single) {
        append_str(buf, len, shader_item_to_str(c[only_alpha][3], with_alpha, only_alpha, opt_alpha, first_cycle, false));
    } else if (do_multiply) {
        append_str(buf, len, shader_item_to_str(c[only_alpha][0], with_alpha, only_alpha, opt_alpha, first_cycle, false));
        append_str(buf, len, " * ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][2], with_alpha, only_alpha, opt_alpha, first_cycle, true));
    } else if (do_mix) {
        append_str(buf, len, "mix(");
        append_str(buf, len, shader_item_to_str(c[only_alpha][1], with_alpha, only_alpha, opt_alpha, first_cycle, false));
        append_str(buf, len, ", ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][0], with_alpha, only_alpha, opt_alpha, first_cycle, false));
        append_str(buf, len, ", ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][2], with_alpha, only_alpha, opt_alpha, first_cycle, true));
        append_str(buf, len, ")");
    } else {
        append_str(buf, len, "(");
        append_str(buf, len, shader_item_to_str(c[only_alpha][0], with_alpha, only_alpha, opt_alpha, first_cycle, false));
        append_str(buf, len, " - ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][1], with_alpha, only_alpha, opt_alpha, first_cycle, false));
        append_str(buf, len, ") * ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][2], with_alpha, only_alpha, opt_alpha, first_cycle, true));
        append_str(buf, len, " + ");
        append_str(buf, len, shader_item_to_str(c[only_alpha][3], with_alpha, only_alpha, opt_alpha, first_cycle, false));
    }
}

static std::string BuildVsShaderInline(const CCFeatures& cc_features, size_t& out_num_floats) {
    char vs_buf[4096];
    size_t vs_len = 0;
    size_t num_floats = 4;

    append_line(vs_buf, &vs_len, "attribute vec4 aVtxPos;");

    for (int i = 0; i < 2; i++) {
        if (cc_features.usedTextures[i]) {
            vs_len += sprintf(vs_buf + vs_len, "attribute vec2 aTexCoord%d;\n", i);
            vs_len += sprintf(vs_buf + vs_len, "varying vec2 vTexCoord%d;\n", i);
            num_floats += 2;
            for (int j = 0; j < 2; j++) {
                if (cc_features.clamp[i][j]) {
                    vs_len += sprintf(vs_buf + vs_len, "attribute float aTexClamp%s%d;\n", j == 0 ? "S" : "T", i);
                    vs_len += sprintf(vs_buf + vs_len, "varying float vTexClamp%s%d;\n", j == 0 ? "S" : "T", i);
                    num_floats += 1;
                }
            }
        }
    }

    if (cc_features.opt_fog) {
        append_line(vs_buf, &vs_len, "attribute vec4 aFog;");
        append_line(vs_buf, &vs_len, "varying vec4 vFog;");
        num_floats += 4;
    }

    if (cc_features.opt_grayscale) {
        append_line(vs_buf, &vs_len, "attribute vec4 aGrayscaleColor;");
        append_line(vs_buf, &vs_len, "varying vec4 vGrayscaleColor;");
        num_floats += 4;
    }

    for (int i = 0; i < cc_features.numInputs; i++) {
        vs_len += sprintf(vs_buf + vs_len, "attribute vec%d aInput%d;\n", cc_features.opt_alpha ? 4 : 3, i + 1);
        vs_len += sprintf(vs_buf + vs_len, "varying vec%d vInput%d;\n", cc_features.opt_alpha ? 4 : 3, i + 1);
        num_floats += cc_features.opt_alpha ? 4 : 3;
    }

    append_line(vs_buf, &vs_len, "void main() {");

    for (int i = 0; i < 2; i++) {
        if (cc_features.usedTextures[i]) {
            vs_len += sprintf(vs_buf + vs_len, "vTexCoord%d = aTexCoord%d;\n", i, i);
            for (int j = 0; j < 2; j++) {
                if (cc_features.clamp[i][j]) {
                    vs_len += sprintf(vs_buf + vs_len, "vTexClamp%s%d = aTexClamp%s%d;\n",
                                      j == 0 ? "S" : "T", i, j == 0 ? "S" : "T", i);
                }
            }
        }
    }

    if (cc_features.opt_fog)       append_line(vs_buf, &vs_len, "vFog = aFog / 255.f;");
    if (cc_features.opt_grayscale)  append_line(vs_buf, &vs_len, "vGrayscaleColor = aGrayscaleColor / 255.f;");

    for (int i = 0; i < cc_features.numInputs; i++) {
        vs_len += sprintf(vs_buf + vs_len, "vInput%d = aInput%d;\n", i + 1, i + 1);
    }

    append_line(vs_buf, &vs_len, "gl_Position = aVtxPos;");
    append_line(vs_buf, &vs_len, "gl_Position.z *= 0.3f;");

    append_line(vs_buf, &vs_len, "}");

    vs_buf[vs_len] = '\0';
    out_num_floats = num_floats;
    return std::string(vs_buf, vs_len);
}

static std::string BuildFsShaderInline(const CCFeatures& cc_features, FilteringMode filter_mode, bool srgb_mode) {
    char fs_buf[16384];
    size_t fs_len = 0;

    for (int i = 0; i < 2; i++) {
        if (cc_features.usedTextures[i]) {
            fs_len += sprintf(fs_buf + fs_len, "varying vec2 vTexCoord%d;\n", i);
            for (int j = 0; j < 2; j++) {
                if (cc_features.clamp[i][j]) {
                    fs_len += sprintf(fs_buf + fs_len, "varying float vTexClamp%s%d;\n", j == 0 ? "S" : "T", i);
                }
            }
        }
    }

    if (cc_features.opt_fog) {
        append_line(fs_buf, &fs_len, "varying vec4 vFog;");
    }

    if (cc_features.opt_grayscale) {
        append_line(fs_buf, &fs_len, "varying vec4 vGrayscaleColor;");
    }

    for (int i = 0; i < cc_features.numInputs; i++) {
        fs_len += sprintf(fs_buf + fs_len, "varying vec%d vInput%d;\n", cc_features.opt_alpha ? 4 : 3, i + 1);
    }

    if (cc_features.usedTextures[0]) append_line(fs_buf, &fs_len, "uniform sampler2D uTex0;");
    if (cc_features.usedTextures[1]) append_line(fs_buf, &fs_len, "uniform sampler2D uTex1;");
    if (cc_features.used_masks[0])   append_line(fs_buf, &fs_len, "uniform sampler2D uTexMask0;");
    if (cc_features.used_masks[1])   append_line(fs_buf, &fs_len, "uniform sampler2D uTexMask1;");
    if (cc_features.used_blend[0])   append_line(fs_buf, &fs_len, "uniform sampler2D uTexBlend0;");
    if (cc_features.used_blend[1])   append_line(fs_buf, &fs_len, "uniform sampler2D uTexBlend1;");

    append_line(fs_buf, &fs_len, "uniform int frame_count;");
    append_line(fs_buf, &fs_len, "uniform float noise_scale;");

    append_line(fs_buf, &fs_len, "float random(in vec3 value) {");
    append_line(fs_buf, &fs_len, "    float _random = dot(sin(value), vec3(12.9898, 78.233, 37.719));");
    append_line(fs_buf, &fs_len, "    return fract(sin(_random) * 143758.5453);");
    append_line(fs_buf, &fs_len, "}");

    if (filter_mode == FILTER_THREE_POINT) {
        append_line(fs_buf, &fs_len, "#define TEX_OFFSET(off) texture2D(tex, texCoord - (off)/texSize)");
        append_line(fs_buf, &fs_len, "vec4 filter3point(in sampler2D tex, in vec2 texCoord, in vec2 texSize) {");
        append_line(fs_buf, &fs_len, "    vec2 offset = fract(texCoord*texSize - vec2(0.5));");
        append_line(fs_buf, &fs_len, "    offset -= step(1.0, offset.x + offset.y);");
        append_line(fs_buf, &fs_len, "    vec4 c0 = TEX_OFFSET(offset);");
        append_line(fs_buf, &fs_len, "    vec4 c1 = TEX_OFFSET(vec2(offset.x - sign(offset.x), offset.y));");
        append_line(fs_buf, &fs_len, "    vec4 c2 = TEX_OFFSET(vec2(offset.x, offset.y - sign(offset.y)));");
        append_line(fs_buf, &fs_len, "    return c0 + abs(offset.x)*(c1-c0) + abs(offset.y)*(c2-c0);");
        append_line(fs_buf, &fs_len, "}");
        append_line(fs_buf, &fs_len, "vec4 hookTexture2D(in sampler2D tex, in vec2 uv, in vec2 texSize) {");
        append_line(fs_buf, &fs_len, "    return filter3point(tex, uv, texSize);");
        append_line(fs_buf, &fs_len, "}");
    } else {
        append_line(fs_buf, &fs_len, "vec4 hookTexture2D(in sampler2D tex, in vec2 uv, in vec2 texSize) {");
        append_line(fs_buf, &fs_len, "    return texture2D(tex, uv);");
        append_line(fs_buf, &fs_len, "}");
    }

    if (srgb_mode) {
        append_line(fs_buf, &fs_len, "vec4 fromLinear(vec4 linearRGB){");
        append_line(fs_buf, &fs_len, "    bvec3 cutoff = lessThan(linearRGB.rgb, vec3(0.0031308));");
        append_line(fs_buf, &fs_len, "    vec3 higher = vec3(1.055)*pow(linearRGB.rgb, vec3(1.0/2.4)) - vec3(0.055);");
        append_line(fs_buf, &fs_len, "    vec3 lower = linearRGB.rgb * vec3(12.92);");
        append_line(fs_buf, &fs_len, "    return vec4(mix(higher, lower, cutoff), linearRGB.a);}");
    }

    append_line(fs_buf, &fs_len, "void main() {");
    append_line(fs_buf, &fs_len, "#define WRAP(x, low, high) mod((x)-(low), (high)-(low)) + (low)");

    for (int i = 0; i < 2; i++) {
        if (cc_features.usedTextures[i]) {
            bool s = cc_features.clamp[i][0], t = cc_features.clamp[i][1];
            fs_len += sprintf(fs_buf + fs_len, "vec2 texSize%d = textureSize(uTex%d, 0);\n", i, i);

            if (!s && !t) {
                fs_len += sprintf(fs_buf + fs_len, "vec2 vTexCoordAdj%d = vTexCoord%d;\n", i, i);
            } else if (s && t) {
                fs_len += sprintf(fs_buf + fs_len,
                    "vec2 vTexCoordAdj%d = clamp(vTexCoord%d, 0.5 / texSize%d, vec2(vTexClampS%d, vTexClampT%d));\n",
                    i, i, i, i, i);
            } else if (s) {
                fs_len += sprintf(fs_buf + fs_len,
                    "vec2 vTexCoordAdj%d = vec2(clamp(vTexCoord%d.s, 0.5 / texSize%d.s, vTexClampS%d), vTexCoord%d.t);\n",
                    i, i, i, i, i);
            } else {
                fs_len += sprintf(fs_buf + fs_len,
                    "vec2 vTexCoordAdj%d = vec2(vTexCoord%d.s, clamp(vTexCoord%d.t, 0.5 / texSize%d.t, vTexClampT%d));\n",
                    i, i, i, i, i);
            }

            fs_len += sprintf(fs_buf + fs_len,
                "vec4 texVal%d = hookTexture2D(uTex%d, vTexCoordAdj%d, texSize%d);\n", i, i, i, i);

            if (cc_features.used_masks[i]) {
                fs_len += sprintf(fs_buf + fs_len, "vec2 maskSize%d = textureSize(uTexMask%d, 0);\n", i, i);
                fs_len += sprintf(fs_buf + fs_len,
                    "vec4 maskVal%d = hookTexture2D(uTexMask%d, vTexCoordAdj%d, maskSize%d);\n", i, i, i, i);

                if (cc_features.used_blend[i]) {
                    fs_len += sprintf(fs_buf + fs_len,
                        "vec4 blendVal%d = hookTexture2D(uTexBlend%d, vTexCoordAdj%d, texSize%d);\n", i, i, i, i);
                } else {
                    fs_len += sprintf(fs_buf + fs_len, "vec4 blendVal%d = vec4(0, 0, 0, 0);\n", i);
                }
                fs_len += sprintf(fs_buf + fs_len,
                    "texVal%d = mix(texVal%d, blendVal%d, maskVal%d.a);\n", i, i, i, i);
            }
        }
    }

    append_line(fs_buf, &fs_len, cc_features.opt_alpha ? "vec4 texel;" : "vec3 texel;");

    for (int c = 0; c < (cc_features.opt_2cyc ? 2 : 1); c++) {
        if (c == 1) {
            if (cc_features.opt_alpha) {
                if (cc_features.c[c][1][2] == SHADER_COMBINED)
                    append_line(fs_buf, &fs_len, "texel.a = WRAP(texel.a, -1.01, 1.01);");
                else
                    append_line(fs_buf, &fs_len, "texel.a = WRAP(texel.a, -0.51, 1.51);");
            }
            if (cc_features.c[c][0][2] == SHADER_COMBINED)
                append_line(fs_buf, &fs_len, "texel.rgb = WRAP(texel.rgb, -1.01, 1.01);");
            else
                append_line(fs_buf, &fs_len, "texel.rgb = WRAP(texel.rgb, -0.51, 1.51);");
        }

        append_str(fs_buf, &fs_len, "texel = ");
        if (!cc_features.color_alpha_same[c] && cc_features.opt_alpha) {
            append_str(fs_buf, &fs_len, "vec4(");
            append_formula(fs_buf, &fs_len, cc_features.c[c],
                           cc_features.do_single[c][0], cc_features.do_multiply[c][0], cc_features.do_mix[c][0],
                           false, false, true, c == 0);
            append_str(fs_buf, &fs_len, ", ");
            append_formula(fs_buf, &fs_len, cc_features.c[c],
                           cc_features.do_single[c][1], cc_features.do_multiply[c][1], cc_features.do_mix[c][1],
                           true, true, true, c == 0);
            append_str(fs_buf, &fs_len, ")");
        } else {
            append_formula(fs_buf, &fs_len, cc_features.c[c],
                           cc_features.do_single[c][0], cc_features.do_multiply[c][0], cc_features.do_mix[c][0],
                           cc_features.opt_alpha, false, cc_features.opt_alpha, c == 0);
        }
        append_line(fs_buf, &fs_len, ";");
    }

    append_line(fs_buf, &fs_len, "texel = WRAP(texel, -0.51, 1.51);");
    append_line(fs_buf, &fs_len, "texel = clamp(texel, 0.0, 1.0);");

    if (cc_features.opt_fog) {
        if (cc_features.opt_alpha)
            append_line(fs_buf, &fs_len, "texel = vec4(mix(texel.rgb, vFog.rgb, vFog.a), texel.a);");
        else
            append_line(fs_buf, &fs_len, "texel = mix(texel, vFog.rgb, vFog.a);");
    }

    if (cc_features.opt_texture_edge && cc_features.opt_alpha)
        append_line(fs_buf, &fs_len, "if (texel.a > 0.19) texel.a = 1.0; else discard;");

    if (cc_features.opt_alpha && cc_features.opt_noise)
        append_line(fs_buf, &fs_len,
            "texel.a *= floor(clamp(random(vec3(floor(gl_FragCoord.xy * noise_scale), float(frame_count))) + texel.a, 0.0, 1.0));");

    if (cc_features.opt_grayscale) {
        append_line(fs_buf, &fs_len, "float intensity = (texel.r + texel.g + texel.b) / 3.0;");
        append_line(fs_buf, &fs_len, "vec3 new_texel = vGrayscaleColor.rgb * intensity;");
        append_line(fs_buf, &fs_len, "texel.rgb = mix(texel.rgb, new_texel, vGrayscaleColor.a);");
    }

    if (cc_features.opt_alpha) {
        if (cc_features.opt_alpha_threshold)
            append_line(fs_buf, &fs_len, "if (texel.a < 8.0 / 256.0) discard;");
        if (cc_features.opt_invisible)
            append_line(fs_buf, &fs_len, "texel.a = 0.0;");
        append_line(fs_buf, &fs_len, "gl_FragColor = texel;");
    } else {
        append_line(fs_buf, &fs_len, "gl_FragColor = vec4(texel, 1.0);");
    }

    if (srgb_mode) {
        append_line(fs_buf, &fs_len, "gl_FragColor = fromLinear(gl_FragColor);");
    }

    append_line(fs_buf, &fs_len, "}");

    fs_buf[fs_len] = '\0';
    return std::string(fs_buf, fs_len);
}

void GfxRenderingAPIOGL::ClearShaderCache() {
}

ShaderProgram* GfxRenderingAPIOGL::CreateAndLoadNewShader(uint64_t shader_id0, uint64_t shader_id1) {
    CCFeatures cc_features;
    gfx_cc_get_features(shader_id0, shader_id1, &cc_features);

    size_t num_floats = 0;
    const std::string vs_buf = BuildVsShaderInline(cc_features, num_floats);
    const std::string fs_buf = BuildFsShaderInline(cc_features, mCurrentFilterMode, mSrgbMode);

    const GLchar* sources[2] = { vs_buf.data(), fs_buf.data() };
    const GLint  lengths[2]  = { (GLint)vs_buf.size(), (GLint)fs_buf.size() };
    GLint success;

    GLuint shader_program = 0;
    int prog_size = 0, prog_len = 0;
    unsigned int prog_format = 0;
    void* prog_bin = nullptr;
    char fname[256];
    sprintf(fname, "ux0:data/DaedalusX64/ShaderCache_F3D/%016llX_%016llX_%d.bin", shader_id1, shader_id0, SHADER_MAGIC);
    FILE* f = fopen(fname, "rb");
    if (f) {
        shader_program = glCreateProgram();
        fseek(f, 0, SEEK_END);
        int file_size = ftell(f);
        fseek(f, 0, SEEK_SET);
        prog_bin = malloc(file_size - sizeof(size_t));
        fread(&num_floats, 1, sizeof(size_t), f);
        fread(prog_bin, 1, file_size - sizeof(size_t), f);
        fclose(f);
        glProgramBinary(shader_program, 0, prog_bin, file_size - sizeof(size_t));
        free(prog_bin);
        goto program_ready;
    }

    {
        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &sources[0], &lengths[0]);
        glCompileShader(vertex_shader);
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            abort();
        }

        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &sources[1], &lengths[1]);
        glCompileShader(fragment_shader);
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            abort();
        }

        shader_program = glCreateProgram();
        glAttachShader(shader_program, vertex_shader);
        glAttachShader(shader_program, fragment_shader);
        glLinkProgram(shader_program);

        f = fopen(fname, "wb");
        if (f) {
            glGetProgramiv(shader_program, GL_PROGRAM_BINARY_LENGTH, &prog_size);
            prog_bin = malloc(prog_size);
            glGetProgramBinary(shader_program, prog_size, &prog_len, &prog_format, prog_bin);
            fwrite(&num_floats, 1, sizeof(size_t), f);
            fwrite(prog_bin, 1, prog_len, f);
            fclose(f);
            free(prog_bin);
        }
    }

program_ready:
    size_t cnt = 0;

    struct ShaderProgram* prg = &mShaderProgramPool[std::make_pair(shader_id0, shader_id1)];
    prg->attribLocations[cnt] = glGetAttribLocation(shader_program, "aVtxPos");
    prg->attribSizes[cnt] = 4;
    ++cnt;

    for (int i = 0; i < 2; i++) {
        if (cc_features.usedTextures[i]) {
            char name[32];
            snprintf(name, sizeof(name), "aTexCoord%d", i);
            prg->attribLocations[cnt] = glGetAttribLocation(shader_program, name);
            prg->attribSizes[cnt] = 2;
            ++cnt;

            for (int j = 0; j < 2; j++) {
                if (cc_features.clamp[i][j]) {
                    snprintf(name, sizeof(name), "aTexClamp%s%d", j == 0 ? "S" : "T", i);
                    prg->attribLocations[cnt] = glGetAttribLocation(shader_program, name);
                    prg->attribSizes[cnt] = 1;
                    ++cnt;
                }
            }
        }
    }

    if (cc_features.opt_fog) {
        prg->attribLocations[cnt] = glGetAttribLocation(shader_program, "aFog");
        prg->attribSizes[cnt] = 4;
        ++cnt;
    }

    if (cc_features.opt_grayscale) {
        prg->attribLocations[cnt] = glGetAttribLocation(shader_program, "aGrayscaleColor");
        prg->attribSizes[cnt] = 4;
        ++cnt;
    }

    for (int i = 0; i < cc_features.numInputs; i++) {
        char name[16];
        snprintf(name, sizeof(name), "aInput%d", i + 1);
        prg->attribLocations[cnt] = glGetAttribLocation(shader_program, name);
        prg->attribSizes[cnt] = cc_features.opt_alpha ? 4 : 3;
        ++cnt;
    }

    prg->openglProgramId = shader_program;
    prg->numInputs = cc_features.numInputs;
    prg->usedTextures[0] = cc_features.usedTextures[0];
    prg->usedTextures[1] = cc_features.usedTextures[1];
    prg->usedTextures[2] = cc_features.used_masks[0];
    prg->usedTextures[3] = cc_features.used_masks[1];
    prg->usedTextures[4] = cc_features.used_blend[0];
    prg->usedTextures[5] = cc_features.used_blend[1];
    prg->numFloats = num_floats;
    prg->numAttribs = cnt;

    prg->frameCountLocation = glGetUniformLocation(shader_program, "frame_count");
    prg->noiseScaleLocation = glGetUniformLocation(shader_program, "noise_scale");

    prg->texture_width_location     = -1;
    prg->texture_height_location    = -1;
    prg->texture_filtering_location = -1;

    LoadShader(prg);

    if (cc_features.usedTextures[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTex0");
        glUniform1i(sampler_location, 0);
    }
    if (cc_features.usedTextures[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTex1");
        glUniform1i(sampler_location, 1);
    }
    if (cc_features.used_masks[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexMask0");
        glUniform1i(sampler_location, 2);
    }
    if (cc_features.used_masks[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexMask1");
        glUniform1i(sampler_location, 3);
    }
    if (cc_features.used_blend[0]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexBlend0");
        glUniform1i(sampler_location, 4);
    }
    if (cc_features.used_blend[1]) {
        GLint sampler_location = glGetUniformLocation(shader_program, "uTexBlend1");
        glUniform1i(sampler_location, 5);
    }

    return prg;
}

struct ShaderProgram* GfxRenderingAPIOGL::LookupShader(uint64_t shader_id0, uint64_t shader_id1) {
    auto it = mShaderProgramPool.find(std::make_pair(shader_id0, shader_id1));
    return it == mShaderProgramPool.end() ? nullptr : &it->second;
}

void GfxRenderingAPIOGL::ShaderGetInfo(struct ShaderProgram* prg, uint8_t* numInputs, bool usedTextures[2]) {
    *numInputs = prg->numInputs;
    usedTextures[0] = prg->usedTextures[0];
    usedTextures[1] = prg->usedTextures[1];
}

GLuint GfxRenderingAPIOGL::NewTexture() {
    GLuint ret;
    glGenTextures(1, &ret);
    return ret;
}

void GfxRenderingAPIOGL::DeleteTexture(uint32_t texID) {
    glDeleteTextures(1, &texID);
}

void GfxRenderingAPIOGL::SelectTexture(int tile, GLuint texture_id) {
    if (mLastActiveTexture != tile) {
        mLastActiveTexture = tile;
        glActiveTexture(GL_TEXTURE0 + tile);
    }
    if (mLastBoundTextures[tile] != texture_id) {
        mLastBoundTextures[tile] = texture_id;
        glBindTexture(GL_TEXTURE_2D, texture_id);
    }
    mCurrentTextureIds[tile] = texture_id;
    mCurrentTile = tile;
}

void GfxRenderingAPIOGL::UploadTexture(const uint8_t* rgba32_buf, uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return;
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba32_buf);
}

static uint32_t gfx_cm_to_opengl(uint32_t val) {
    switch (val) {
        case G_TX_NOMIRROR | G_TX_CLAMP:
            return GL_CLAMP_TO_EDGE;
        case G_TX_MIRROR | G_TX_WRAP:
            return GL_MIRRORED_REPEAT;
        case G_TX_MIRROR | G_TX_CLAMP:
            return GL_MIRROR_CLAMP_EXT;
        case G_TX_NOMIRROR | G_TX_WRAP:
            return GL_REPEAT;
    }
    return 0;
}

void GfxRenderingAPIOGL::SetSamplerParameters(int tile, bool linear_filter, uint32_t cms, uint32_t cmt) {
    if (mLastActiveTexture != tile) {
        mLastActiveTexture = tile;
        glActiveTexture(GL_TEXTURE0 + tile);
    }
    const GLint filter = linear_filter && mCurrentFilterMode == FILTER_LINEAR ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gfx_cm_to_opengl(cms));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gfx_cm_to_opengl(cmt));
}

void GfxRenderingAPIOGL::SetDepthTestAndMask(bool depth_test, bool z_upd) {
    mCurrentDepthTest = depth_test;
    mCurrentDepthMask = z_upd;
}

void GfxRenderingAPIOGL::SetCurrentPrimDepth(float depth) {
    if (depth != mCurrentPrimDepth) {
        mCurrentPrimDepth = depth;
        mPrimDepthDirty = true;
    }
}

void GfxRenderingAPIOGL::SetZmodeDecal(bool zmode_decal) {
    mCurrentZmodeDecal = zmode_decal;
}

void GfxRenderingAPIOGL::SetViewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

void GfxRenderingAPIOGL::SetScissor(int x, int y, int width, int height) {
    glScissor(x, y, width, height);
}

void GfxRenderingAPIOGL::SetUseAlpha(bool use_alpha) {
    int8_t val = use_alpha ? 1 : 0;
    if (mLastBlendEnabled != val) {
        mLastBlendEnabled = val;
        if (use_alpha) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
    }
}

void GfxRenderingAPIOGL::DrawTriangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    if (mCurrentDepthTest != mLastDepthTest || mCurrentDepthMask != mLastDepthMask) {
        mLastDepthTest = mCurrentDepthTest;
        mLastDepthMask = mCurrentDepthMask;

        if (mCurrentDepthTest || mLastDepthMask) {
            glEnable(GL_DEPTH_TEST);
            glDepthMask(mLastDepthMask ? GL_TRUE : GL_FALSE);
            glDepthFunc(mCurrentDepthTest ? (mCurrentZmodeDecal ? GL_LEQUAL : GL_LESS) : GL_ALWAYS);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }

    if (mCurrentZmodeDecal != mLastZmodeDecal) {
        mLastZmodeDecal = mCurrentZmodeDecal;
        if (mCurrentZmodeDecal) {
            // SSDB = SlopeScaledDepthBias 120 leads to -2 at 240p which is the same as N64 mode which has very little
            // fighting
            const int n64modeFactor = 120;
            const int noVanishFactor = 100;
            GLfloat SSDB = -2;
#if 0
			// FIXME: Properly reimplement this
            switch (Ship::Context::GetInstance()->GetConsoleVariables()->GetInteger(CVAR_Z_FIGHTING_MODE, 0)) {
                // scaled z-fighting (N64 mode like)
                case 1:
                    if (mFrameBuffers.size() >
                        mCurrentFrameBuffer) { // safety check for vector size can probably be removed
                        SSDB = -1.0f * (GLfloat)mFrameBuffers[mCurrentFrameBuffer].height / n64modeFactor;
                    }
                    break;
                // no vanishing paths
                case 2:
                    if (mFrameBuffers.size() >
                        mCurrentFrameBuffer) { // safety check for vector size can probably be removed
                        SSDB = -1.0f * (GLfloat)mFrameBuffers[mCurrentFrameBuffer].height / noVanishFactor;
                    }
                    break;
                // disabled
                case 0:
                default:
                    SSDB = -2;
            }
#endif
            glPolygonOffset(SSDB, -2);
            glEnable(GL_POLYGON_OFFSET_FILL);
        } else {
            glPolygonOffset(0, 0);
            glDisable(GL_POLYGON_OFFSET_FILL);
        }
    }

    SetPerDrawUniforms();

    vglBufferData(GL_ARRAY_BUFFER, buf_vbo);
    glDrawArrays(GL_TRIANGLES, 0, 3 * buf_vbo_num_tris);
}

void GfxRenderingAPIOGL::Init() {
    glGenBuffers(1, &mOpenglVbo);
    glBindBuffer(GL_ARRAY_BUFFER, mOpenglVbo);

    glDepthFunc(GL_LEQUAL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    mFrameBuffers.resize(1); // for the default screen buffer

    glGenRenderbuffers(1, &mPixelDepthRb);
    glBindRenderbuffer(GL_RENDERBUFFER, mPixelDepthRb);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    glGenFramebuffers(1, &mPixelDepthFb);
    glBindFramebuffer(GL_FRAMEBUFFER, mPixelDepthFb);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, mPixelDepthRb);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    mPixelDepthRbSize = 1;
}

void GfxRenderingAPIOGL::OnResize() {
}

void GfxRenderingAPIOGL::StartFrame() {
    mFrameCount++;
}

void GfxRenderingAPIOGL::EndFrame() {
}

void GfxRenderingAPIOGL::FinishRender() {
}

int GfxRenderingAPIOGL::CreateFramebuffer() {
    GLuint clrbuf;
    glGenTextures(1, &clrbuf);
    glBindTexture(GL_TEXTURE_2D, clrbuf);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint clrbufMsaa;
    glGenRenderbuffers(1, &clrbufMsaa);

    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 1, 1);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);

    GLuint fbo;
    glGenFramebuffers(1, &fbo);

    size_t i = mFrameBuffers.size();
    mFrameBuffers.resize(i + 1);

    mFrameBuffers[i].fbo = fbo;
    mFrameBuffers[i].clrbuf = clrbuf;
    mFrameBuffers[i].clrbufMsaa = clrbufMsaa;
    mFrameBuffers[i].rbo = rbo;

    return i;
}

void GfxRenderingAPIOGL::UpdateFramebufferParameters(int fb_id, uint32_t width, uint32_t height,
                                                     bool opengl_invertY, bool render_target, bool has_depth_buffer,
                                                     bool can_extract_depth) {
    FramebufferOGL& fb = mFrameBuffers[fb_id];

    width = std::max(width, 1U);
    height = std::max(height, 1U);

    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);

    if (fb_id != 0) {
        if (fb.width != width || fb.height != height) {
            glBindTexture(GL_TEXTURE_2D, fb.clrbuf);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
            glBindTexture(GL_TEXTURE_2D, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.clrbuf, 0);
        }

        if (has_depth_buffer &&
            (fb.width != width || fb.height != height || !fb.has_depth_buffer)) {
            glBindRenderbuffer(GL_RENDERBUFFER, fb.rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        if (!fb.has_depth_buffer && has_depth_buffer) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, fb.rbo);
        } else if (fb.has_depth_buffer && !has_depth_buffer) {
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
        }
    }

    fb.width = width;
    fb.height = height;
    fb.has_depth_buffer = has_depth_buffer;
    fb.invertY = opengl_invertY;
}

void GfxRenderingAPIOGL::StartDrawToFramebuffer(int fb_id, float noise_scale) {
    FramebufferOGL& fb = mFrameBuffers[fb_id];

    if (noise_scale != 0.0f) {
        mCurrentNoiseScale = 1.0f / noise_scale;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);

    uint32_t overrideFb = DaedalusFast3D_GetFramebufferOverride(fb_id);
    if (overrideFb != 0) glBindFramebuffer(GL_FRAMEBUFFER, overrideFb);

    mCurrentFrameBuffer = fb_id;
}

void GfxRenderingAPIOGL::ClearFramebuffer(bool color, bool depth) {
    if (DaedalusFast3D_ShouldSuppressFramebufferClear(color, depth)) return;

    if (mLastScissorEnabled != 0) {
        mLastScissorEnabled = 0;
        glDisable(GL_SCISSOR_TEST);
    }
    glDepthMask(GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear((color ? GL_COLOR_BUFFER_BIT : 0) | (depth ? GL_DEPTH_BUFFER_BIT : 0));
    glDepthMask(mCurrentDepthMask ? GL_TRUE : GL_FALSE);
    if (mLastScissorEnabled != 1) {
        mLastScissorEnabled = 1;
        glEnable(GL_SCISSOR_TEST);
    }
}

void GfxRenderingAPIOGL::ClearDepthRegion(int x, int y, int w, int h) {
    // Save current scissor state so callers don't need to manually invalidate.
    GLint prevScissor[4];
    GLboolean scissorWasEnabled = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_SCISSOR_BOX, prevScissor);

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
    glDepthMask(GL_TRUE);
    glClear(GL_DEPTH_BUFFER_BIT);
    glDepthMask(mCurrentDepthMask ? GL_TRUE : GL_FALSE);

    // Restore previous scissor state.
    glScissor(prevScissor[0], prevScissor[1], prevScissor[2], prevScissor[3]);
    if (!scissorWasEnabled) {
        glDisable(GL_SCISSOR_TEST);
    }
}

void GfxRenderingAPIOGL::ResolveMSAAColorBuffer(int fb_id_target, int fb_id_source) {
    FramebufferOGL& fb_dst = mFrameBuffers[fb_id_target];
    FramebufferOGL& fb_src = mFrameBuffers[fb_id_source];
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fb_dst.fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fb_src.fbo);

    // Disabled for blit
    if (mLastScissorEnabled != 0) {
        mLastScissorEnabled = 0;
        glDisable(GL_SCISSOR_TEST);
    }

    glBlitFramebuffer(0, 0, fb_src.width, fb_src.height, 0, 0, fb_dst.width, fb_dst.height, GL_COLOR_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, mCurrentFrameBuffer);

    if (mLastScissorEnabled != 1) {
        mLastScissorEnabled = 1;
        glEnable(GL_SCISSOR_TEST);
    }
}

void* GfxRenderingAPIOGL::GetFramebufferTextureId(int fb_id) {
    return (void*)(uintptr_t)mFrameBuffers[fb_id].clrbuf;
}

void GfxRenderingAPIOGL::SelectTextureFb(int fb_id) {
    // glDisable(GL_DEPTH_TEST);
    int tile = 0;
    SelectTexture(tile, mFrameBuffers[fb_id].clrbuf);
}

void GfxRenderingAPIOGL::CopyFramebuffer(int fb_dst_id, int fb_src_id, int srcX0, int srcY0, int srcX1, int srcY1,
                                         int dstX0, int dstY0, int dstX1, int dstY1) {
    if (fb_dst_id >= (int)mFrameBuffers.size() || fb_src_id >= (int)mFrameBuffers.size()) {
        return;
    }

    FramebufferOGL src = mFrameBuffers[fb_src_id];
    const FramebufferOGL& dst = mFrameBuffers[fb_dst_id];

    // Adjust y values for non-inverted source frame buffers because opengl uses bottom left for origin
    if (!src.invertY) {
        int temp = srcY1 - srcY0;
        srcY1 = src.height - srcY0;
        srcY0 = srcY1 - temp;
    }

    // Flip the y values
    if (src.invertY != dst.invertY) {
        std::swap(srcY0, srcY1);
    }

    // Disabled for blit
    if (mLastScissorEnabled != 0) {
        mLastScissorEnabled = 0;
        glDisable(GL_SCISSOR_TEST);
    }

    GLuint srcFbo = src.fbo;
    GLuint dstFbo = dst.fbo;
    GLuint currentFbo = mFrameBuffers[mCurrentFrameBuffer].fbo;

    const uint32_t srcOverride = DaedalusFast3D_GetFramebufferOverride(fb_src_id);
    const uint32_t dstOverride = DaedalusFast3D_GetFramebufferOverride(fb_dst_id);
    const uint32_t currentOverride = DaedalusFast3D_GetFramebufferOverride(mCurrentFrameBuffer);
    if (srcOverride != 0) srcFbo = srcOverride;
    if (dstOverride != 0) dstFbo = dstOverride;
    if (currentOverride != 0) currentFbo = currentOverride;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dstFbo);

    glReadBuffer(srcFbo == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);
    glBlitFramebuffer(srcX0, srcY0, srcX1, srcY1, dstX0, dstY0, dstX1, dstY1, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, currentFbo);

    glReadBuffer(currentFbo == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);

    if (mLastScissorEnabled != 1) {
        mLastScissorEnabled = 1;
        glEnable(GL_SCISSOR_TEST);
    }
}

void GfxRenderingAPIOGL::ReadFramebufferToCPU(int fb_id, uint32_t width, uint32_t height, uint16_t* rgba16_buf) {
    if (fb_id >= (int)mFrameBuffers.size() || rgba16_buf == nullptr) {
        return;
    }

    GLuint readFbo = mFrameBuffers[fb_id].fbo;
    GLuint currentFbo = mFrameBuffers[mCurrentFrameBuffer].fbo;

    const uint32_t readOverride = DaedalusFast3D_GetFramebufferOverride(fb_id);
    const uint32_t currentOverride = DaedalusFast3D_GetFramebufferOverride(mCurrentFrameBuffer);
    if (readOverride != 0) readFbo = readOverride;
    if (currentOverride != 0) currentFbo = currentOverride;

    glBindFramebuffer(GL_FRAMEBUFFER, readFbo);
    glReadBuffer(readFbo == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1, rgba16_buf);

    glBindFramebuffer(GL_FRAMEBUFFER, currentFbo);
    glReadBuffer(currentFbo == 0 ? GL_BACK : GL_COLOR_ATTACHMENT0);
}

std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff>
GfxRenderingAPIOGL::GetPixelDepth(int fb_id, const std::set<std::pair<float, float>>& coordinates) {
    std::unordered_map<std::pair<float, float>, uint16_t, hash_pair_ff> res;
	// FIXME: Needs to be implemented with vglTexImageDepthBuffer
    return res;
}

void GfxRenderingAPIOGL::SetTextureFilter(FilteringMode mode) {
    gfx_texture_cache_clear();
    mCurrentFilterMode = mode;
}

FilteringMode GfxRenderingAPIOGL::GetTextureFilter() {
    return mCurrentFilterMode;
}

void GfxRenderingAPIOGL::SetSrgbMode() {
    mSrgbMode = true;
}

ImTextureID GfxRenderingAPIOGL::GetTextureById(int id) {
    return reinterpret_cast<ImTextureID>(id);
}

void GfxRenderingAPIOGL::InvalidateExternalState() {
    mCurrentShaderProgram = nullptr;
    mLastLoadedShader = nullptr;
    mLastActiveTexture = -1;
    mLastBlendEnabled = -1;
    mLastScissorEnabled = -1;
    mLastDepthTest = -1;
    mLastDepthMask = -1;
    mLastZmodeDecal = -1;
    for (int i = 0; i < SHADER_MAX_TEXTURES; ++i) mLastBoundTextures[i] = 0xFFFFFFFFu;
}
} // namespace Fast
#endif

#pragma clang diagnostic pop
