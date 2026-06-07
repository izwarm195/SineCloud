/* ==============================================================================
   PostPipeline.h
   Layer 3: Deferred Rendering
   全屏四边形延迟光照 pass + 后续后处理框架。
   ============================================================================== */
#pragma once
#include "GLUtils.h"
#include "Shader.h"
#include "Lighting.h"
#include "Camera.h"
#include "PointLight.h"
#include "Mat4.h"

namespace sc {

    class GBuffer;

    class PostPipeline
    {
    public:
        explicit PostPipeline(juce::OpenGLContext& ctx)
            : context(ctx), deferredShader(ctx) {
        }

        // ----------------------------------------------------------------
        // 生命周期
        // ----------------------------------------------------------------
        bool build()
        {
            using namespace sc::gl;

            const juce::String vs = R"(#version 330 core
        layout(location = 0) in vec2 aPos;
        out vec2 vUV;
        void main()
        {
            vUV = aPos * 0.5 + 0.5;
            gl_Position = vec4(aPos, 0.0, 1.0);
        })";

            const juce::String fs = R"(#version 330 core
in vec2 vUV;
uniform sampler2D gAlbedoRough;
uniform sampler2D gNormalMetal;
uniform sampler2D gEmissiveSSS;
uniform sampler2D gDepth; 
uniform sampler2D gWorldPos;
uniform vec3 uCamPos;

uniform vec3 uLightDir;
uniform vec3 uLightColor;
uniform vec3 uAmbient;
uniform vec3 uSkyColor;
uniform vec3 uGroundColor;
uniform float uLightIntensity;

const int MAX_POINT_LIGHTS = 16;
uniform int uNumPointLights;
uniform vec3 uPointLightPos[MAX_POINT_LIGHTS];
uniform vec3 uPointLightColor[MAX_POINT_LIGHTS];
uniform float uPointLightQuadratic[MAX_POINT_LIGHTS];
uniform float uPointLightRange[MAX_POINT_LIGHTS];

uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uFogHeightFalloff;
uniform float uFogStart;
uniform vec3 uPlayerPos;

uniform float uShadeLevels;

// ---- 云层 / 体积光 ----
uniform float uCloudScale;        // Perlin 缩放（如 0.03）
uniform float uCloudThreshold;    // 云/晴 阈值（如 0.45）
uniform float uCloudSpeed;        // 云滚动速度
uniform float uCloudPlaneHeight;  // 虚拟天花板高度（世界空间 Z）
uniform float uCloudTime;         // 当前时间（秒）
uniform float uVolumetricSteps;   // Ray March 步数（如 8.0）
uniform float uVolumetricIntensity; // 体积光强度（如 0.6）
uniform float uCloudBandLevels;   // 阶梯量化级数（如 3.0，用于像素风 banding）


out vec4 fragColor;

const float PI = 3.14159265359;

// ============================================================
// Perlin Noise (2D) — 用于生成云纹理
// ============================================================
float hash21(vec2 p) {
    float h = dot(p, vec2(127.1, 311.7));
    return fract(sin(h) * 43758.5453);
}

float noise2D(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);  // smoothstep

    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float perlin2D(vec2 p) {
    float total = 0.0;
    float amp   = 0.5;
    float freq  = 1.0;
    for (int i = 0; i < 4; ++i) {
        total += amp * noise2D(p * freq);
        freq  *= 2.0;
        amp   *= 0.5;
    }
    return total;  // 范围约 [0, 1]
}


float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float d = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-5);
}
float G_Smith(float NoV, float NoL, float a) {
    float k = (a + 1.0); k = (k * k) * 0.125;
    float gv = NoV / (NoV * (1.0 - k) + k);
    float gl = NoL / (NoL * (1.0 - k) + k);
    return gv * gl;
}
vec3 F_Schlick(float VoH, vec3 F0) {
    float f = pow(1.0 - VoH, 5.0);
    return F0 + (1.0 - F0) * f;
}

vec3 evalBRDF(vec3 N, vec3 V, vec3 L, vec3 baseColor,
              float roughness, float metallic, vec3 F0, vec3 radiance)
{
    vec3 H = normalize(V + L);
    float NoL = max(dot(N, L), 0.0);
    if (NoL <= 0.0) return vec3(0.0);
    float NoV = max(dot(N, V), 1e-4);
    float NoH = max(dot(N, H), 0.0);
    float VoH = max(dot(V, H), 0.0);

    float a = max(roughness * roughness, 0.002);
    float D = D_GGX(NoH, a);
    float G = G_Smith(NoV, NoL, a);
    vec3  F = F_Schlick(VoH, F0);

    vec3 spec = (D * G * F) / max(4.0 * NoL * NoV, 1e-4);
    vec3 kd = (1.0 - F) * (1.0 - metallic);
    vec3 diffuse = kd * baseColor;
    return (diffuse + spec) * radiance * NoL;
}

// ---- 采样云纹理：给定世界 XZ 坐标，返回 [0,1] 覆盖率 ----
float sampleCloud(float wx, float wz, float time) {
    vec2 uv = vec2(wx, wz) * uCloudScale;
    uv.x += time * uCloudSpeed * 0.3;   // X 方向滚动
    uv.y += time * uCloudSpeed * 0.15;  // Y 方向滚动稍慢，产生交错感
    float n = perlin2D(uv);

    // ---- 像素风 banding：阶梯量化代替模糊 ----
    if (uCloudBandLevels > 1.5) {
        n = (floor(n * uCloudBandLevels) + 0.5) / uCloudBandLevels;
    }
    return n;
}

// ---- 云阴影查询：地面上一个点是否被云遮挡 ----
float getCloudShadow(vec3 worldPos, vec3 sunDir, float time) {
    // 线性方程：worldPos + t * sunDir  与平面 Z = uCloudPlaneHeight 的交点
    // Z 分量：worldPos.z + t * sunDir.z = uCloudPlaneHeight
    // => t = (uCloudPlaneHeight - worldPos.z) / sunDir.z

    if (abs(sunDir.z) < 1e-6) return 1.0;  // 光线平行于平面，无遮挡

    float t = (uCloudPlaneHeight - worldPos.z) / sunDir.z;
    if (t <= 0.0) return 1.0;  // 云平面在下方，无遮挡

    vec3 hitPoint = worldPos + t * sunDir;
    float cloud = sampleCloud(hitPoint.x, hitPoint.y, time);

    // 阈值判定：cloud > threshold 表示被云覆盖 → 阴影
    float shadow = 1.0 - smoothstep(uCloudThreshold - 0.05, uCloudThreshold + 0.05, cloud);
    // 映射到 [0.5, 1.0]，让阴影区域至少有 50% 的光照（避免完全黑）
    return mix(0.5, 1.0, shadow);
}

// ---- 体积光 Ray Marching ----
float getVolumetricLight(vec3 worldPos, vec3 camPos, vec3 sunDir, float time) {
    vec3 rayDir = worldPos - camPos;
    float rayLen = length(rayDir);
    if (rayLen < 0.01) return 0.0;
    rayDir /= rayLen;

    int steps = int(uVolumetricSteps);
    float stepSize = rayLen / float(steps);

    float accum = 0.0;  // 累积未被遮挡的步数

    for (int i = 0; i < steps; ++i) {
        float t = (float(i) + 0.5) * stepSize;
        vec3 samplePos = camPos + rayDir * t;

        // 同云阴影查询：该空间点是否能看到太阳
        float tSun = (uCloudPlaneHeight - samplePos.z) / max(abs(sunDir.z), 1e-6);
        if (tSun > 0.0) {
            vec3 hitPoint = samplePos + tSun * sunDir;
            float cloud = sampleCloud(hitPoint.x, hitPoint.y, time);
            // 未被云遮挡 → 该步贡献体积光
            accum += 1.0 - smoothstep(uCloudThreshold - 0.05, uCloudThreshold + 0.05, cloud);
        } else {
            accum += 1.0;  // 在云平面上方，始终可见
        }
    }

    return accum / float(steps);  // [0, 1]，可见比例
}


void main() {
    float depth = texture(gDepth, vUV).r;

    if (depth >= 0.99999) {
        fragColor = vec4(0.06, 0.07, 0.09, 1.0);
        return;
    }

    vec4 ar = texture(gAlbedoRough, vUV);
    vec4 nm = texture(gNormalMetal, vUV);
    vec4 es = texture(gEmissiveSSS, vUV);
    vec3 worldPos = texture(gWorldPos, vUV).rgb;

    vec3 baseColor = ar.rgb;
    float roughness = ar.a;
    vec3 N = normalize(nm.rgb * 2.0 - 1.0);
    float metallic = nm.a;
    vec3 emissive = es.rgb;
    float sss = es.a;

    vec3 V = normalize(uCamPos - worldPos);
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    float NoV = max(dot(N, V), 1e-4);

    // Directional light
    vec3 Ldir = -normalize(uLightDir);
    vec3 dirRad = uLightColor * uLightIntensity;
    vec3 direct = evalBRDF(N, V, Ldir, baseColor, roughness, metallic, F0, dirRad);

    // ★★★ 新增：云阴影遮罩 ★★★
    float cloudShadow = getCloudShadow(worldPos, Ldir, uCloudTime);
    direct *= cloudShadow;  // 方向光被云层减弱

    // ★★★ 新增：体积光 (God Rays) ★★★
    float volumetric = getVolumetricLight(worldPos, uCamPos, Ldir, uCloudTime);
    // 体积光颜色：暖色调散射光
    vec3 volumetricColor = uLightColor * vec3(1.0, 0.95, 0.85);
    vec3 volumetricTerm = volumetricColor * volumetric * uVolumetricIntensity * uLightIntensity;

    // SSS (directional only)
    float backLight = max(dot(-N, Ldir), 0.0);
    vec3 sssTerm = sss * backLight * baseColor * uLightColor * uLightIntensity * 0.5;



    // Point lights
    vec3 pointSum = vec3(0.0);
    for (int i = 0; i < uNumPointLights; ++i) {
        vec3 toLight = uPointLightPos[i] - worldPos;
        float dist = length(toLight);
        if (dist > uPointLightRange[i]) continue;
        vec3 Lp = toLight / max(dist, 1e-4);
        float atten = 1.0 / (1.0 + uPointLightQuadratic[i] * dist * dist);
        float edge = 1.0 - smoothstep(uPointLightRange[i] * 0.85, uPointLightRange[i], dist);
        vec3 radiance = uPointLightColor[i] * atten * edge;
        pointSum += evalBRDF(N, V, Lp, baseColor, roughness, metallic, F0, radiance);
    }

    // Ambient (matches 55a0f17 PixelMaterial)
    float upDot = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 hemi = mix(uGroundColor, uSkyColor, upDot);
    vec3 envIrradiance = uAmbient + hemi;
    vec3 ambientDiffuse = (1.0 - metallic) * baseColor * envIrradiance;
    vec3 F_env = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NoV, 5.0);
    vec3 ambientSpec = F_env * envIrradiance;
    vec3 ambientTerm = ambientDiffuse + ambientSpec;

    // Combine
    vec3 col = direct + pointSum + sssTerm + ambientTerm + emissive + volumetricTerm;

    // Fog
    if (uFogDensity > 0.0) {
        float dPl = length(uPlayerPos.xy - worldPos.xy);
        float dFog = max(dPl - uFogStart, 0.0);
        float heightK = exp(-max(worldPos.z - uPlayerPos.z, 0.0) * uFogHeightFalloff);
        float fogAmt = 1.0 - exp(-dFog * uFogDensity * heightK);
        col = mix(col, uFogColor, clamp(fogAmt, 0.0, 1.0));
    }

    // Shade levels (luminance-preserving, matches 55a0f17)
    if (uShadeLevels > 1.5) {
        float Y = max(dot(col, vec3(0.2126, 0.7152, 0.0722)), 1e-5);
        float Yq = (floor(Y * uShadeLevels) + 0.5) / uShadeLevels;
        col = col * (Yq / Y);
    }

    // Reinhard tonemap + Gamma
    col = col / (col + vec3(1.0));
    col = pow(col, vec3(1.0 / 2.2));
    fragColor = vec4(col, 1.0);
}

)";



            if (!deferredShader.build(vs, fs))
            {
                DBG("PostPipeline: deferred shader build failed");
                return false;
            }

            // ---- 全屏四边形 VAO ----
            const float quad[8] = {
                -1.f, -1.f,   1.f, -1.f,
                -1.f,  1.f,   1.f,  1.f
            };
            glGenVertexArrays(1, &fullscreenVAO);
            glGenBuffers(1, &fullscreenVBO);
            glBindVertexArray(fullscreenVAO);
            glBindBuffer(GL_ARRAY_BUFFER, fullscreenVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            checkError("PostPipeline::build");
            return true;
        }

        void shutdown()
        {
            if (fullscreenVBO) sc::gl::glDeleteBuffers(1, &fullscreenVBO);
            if (fullscreenVAO) sc::gl::glDeleteVertexArrays(1, &fullscreenVAO);
            deferredShader.raw().release();
        }

        // ----------------------------------------------------------------
        // 每帧：延迟光照
        // ----------------------------------------------------------------
        void doLighting(const GBuffer& gbuffer,
            const Camera& camera,
            const Lighting& lighting,
            const Vec3& playerPos,
            float shadeLevels) noexcept
        {
            using namespace sc::gl;

            deferredShader.use();

            gbuffer.bindTexturesForLighting(deferredShader);

            // 重建世界坐标所需
            deferredShader.setVec3("uCamPos", camera.getEyeWorld());

            // ---- 方向光 ----
            deferredShader.setVec3("uLightDir", lighting.direction);
            deferredShader.setVec3("uLightColor", lighting.color);
            deferredShader.setVec3("uAmbient", lighting.ambient);
            deferredShader.setVec3("uSkyColor", lighting.skyColor);
            deferredShader.setVec3("uGroundColor", lighting.groundColor);
            deferredShader.setFloat("uLightIntensity", lighting.intensity);

            // ---- 点光源 ----
            setPointLights(lighting, camera.getEyeWorld());

            // ---- 雾 ----
            setFog(lighting);

            deferredShader.setVec3("uPlayerPos", playerPos);
            deferredShader.setFloat("uShadeLevels", shadeLevels);

            // ---- 云层 / 体积光 ----
            auto setCF = [&](const char* name, float v) {
                deferredShader.setFloat(name, v);
                };
            setCF("uCloudScale", lighting.cloudScale);
            setCF("uCloudThreshold", lighting.cloudThreshold);
            setCF("uCloudSpeed", lighting.cloudSpeed);
            setCF("uCloudPlaneHeight", lighting.cloudPlaneHeight);
            setCF("uCloudBandLevels", lighting.cloudBandLevels);
            setCF("uVolumetricSteps", lighting.volumetricSteps);
            setCF("uVolumetricIntensity", lighting.volumetricIntensity);
            setCF("uCloudTime", lighting.cloudTime);


            // ---- 画全屏四边形 ----
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);

            glBindVertexArray(fullscreenVAO);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            glBindVertexArray(0);

            glDepthMask(GL_TRUE);
            glEnable(GL_DEPTH_TEST);

            checkError("PostPipeline::doLighting");
        }

    private:
        // ---- 点光源上传 ----
        void setPointLights(const Lighting& l, const Vec3& camPos) noexcept
        {
            auto srgbToLin = [](const Vec3& c) {
                auto t = [](float v) {
                    return v <= 0.04045f ? v / 12.92f
                        : std::pow((v + 0.055f) / 1.055f, 2.4f);
                    };
                return Vec3{ t(c.x), t(c.y), t(c.z) };
                };

            struct Tmp { const PointLight* p; float d2; };
            std::vector<Tmp> tmps;
            tmps.reserve(l.pointLights.size());
            for (const auto& pl : l.pointLights)
            {
                if (!pl.enabled) continue;
                float dx = pl.position.x - camPos.x;
                float dy = pl.position.y - camPos.y;
                float dz = pl.position.z - camPos.z;
                tmps.push_back({ &pl, dx * dx + dy * dy + dz * dz });
            }
            std::sort(tmps.begin(), tmps.end(),
                [](const Tmp& a, const Tmp& b) { return a.d2 < b.d2; });

            const int n = juce::jmin((int)tmps.size(), MAX_POINT_LIGHTS);
            deferredShader.setInt("uNumPointLights", n);

            for (int i = 0; i < n; ++i)
            {
                const auto& pl = *tmps[(size_t)i].p;
                const Vec3 colorLin = srgbToLin(pl.colorSRGB) * pl.intensity;

                const auto namePos = juce::String("uPointLightPos[") + juce::String(i) + "]";
                const auto nameCol = juce::String("uPointLightColor[") + juce::String(i) + "]";
                const auto nameRange = juce::String("uPointLightRange[") + juce::String(i) + "]";
                const auto nameQuad = juce::String("uPointLightQuadratic[") + juce::String(i) + "]";

                deferredShader.setVec3(namePos.toRawUTF8(), pl.position);
                deferredShader.setVec3(nameCol.toRawUTF8(), colorLin);
                deferredShader.setFloat(nameRange.toRawUTF8(), pl.range);
                deferredShader.setFloat(nameQuad.toRawUTF8(), pl.quadratic());
            }

            for (int i = n; i < MAX_POINT_LIGHTS; ++i)
{
    const auto nameCol  = juce::String("uPointLightColor[")     + juce::String(i) + "]";
    const auto nameRange = juce::String("uPointLightRange[")    + juce::String(i) + "]";
    const auto nameQuad = juce::String("uPointLightQuadratic[") + juce::String(i) + "]";

    deferredShader.setVec3(nameCol.toRawUTF8(),  Vec3{0,0,0});
    deferredShader.setFloat(nameRange.toRawUTF8(), 0.0f);
    deferredShader.setFloat(nameQuad.toRawUTF8(), 1.0f);
}

        }

        void setFog(const Lighting& l) noexcept
        {
            auto srgbToLin = [](float v) {
                return v <= 0.04045f ? v / 12.92f
                    : std::pow((v + 0.055f) / 1.055f, 2.4f);
                };
            Vec3 fogLin{
                srgbToLin(l.fogColorSRGB.x),
                srgbToLin(l.fogColorSRGB.y),
                srgbToLin(l.fogColorSRGB.z)
            };
            deferredShader.setVec3("uFogColor", fogLin);
            deferredShader.setFloat("uFogDensity", l.fogDensity);
            deferredShader.setFloat("uFogHeightFalloff", l.fogHeightFalloff);
            deferredShader.setFloat("uFogStart", l.fogStart);
        }

        juce::OpenGLContext& context;
        Shader deferredShader;

        GLuint fullscreenVAO = 0;
        GLuint fullscreenVBO = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PostPipeline)
    };

} // namespace sc
