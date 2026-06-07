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

// ============================================================
// 真正的 3D 值噪声 + FBM（时间连续）
// ============================================================

// 3D hash — 每个格点一个固定随机值
float hash3D(vec3 p) {
    float h = dot(p, vec3(127.1, 311.7, 74.7));
    return fract(sin(h) * 43758.5453);
}

// 3D 值噪声 — 三线性插值，保证时间连续
float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);  // smoothstep

    float n000 = hash3D(i);
    float n100 = hash3D(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash3D(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash3D(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash3D(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash3D(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash3D(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash3D(i + vec3(1.0, 1.0, 1.0));

    return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
               mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

// FBM — 多个 octave 叠加，时间作为 Z 轴
float fbm3D(vec3 p) {
    float total = 0.0;
    float amp   = 0.5;
    float freq  = 1.0;

    for (int i = 0; i < 4; ++i) {
        total += amp * noise3D(p * freq);
        freq  *= 2.0;
        amp   *= 0.5;
    }
    return total;  // 约 [0, 1]
}


// ---- 采样云纹理：给定世界 XZ 坐标，返回 [0,1] 覆盖率 ----
float sampleCloud(float wx, float wz, float time) {
    vec2 uv = vec2(wx, wz) * uCloudScale;

    // ---- Domain Warp：微弱扭曲 ----
    float warpStrength = 0.08;
    float warpX = fbm3D(vec3(uv * 2.7,              time * 0.04)) * 2.0 - 1.0;
    float warpY = fbm3D(vec3(uv * 2.7 + 5.2,        time * 0.04)) * 2.0 - 1.0;
    uv += vec2(warpX, warpY) * warpStrength;

    // ---- 全局漂移 ----
    uv.x += time * uCloudSpeed * 0.3;
    uv.y += time * uCloudSpeed * 0.15;

    // ---- 主噪声：Z = time * 0.015，沿时间轴平滑滑动 ----
    float n = fbm3D(vec3(uv, time * 0.015));

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

// ---- 体积光 Ray Marching (修正版) ----
float getVolumetricLight(vec3 worldPos, vec3 camPos, vec3 sunDir, float time) {
    vec3 rayDir = worldPos - camPos;
    float rayLen = length(rayDir);
    if (rayLen < 0.01) return 0.0;
    rayDir /= rayLen;

    int steps = int(uVolumetricSteps);
    steps = max(steps, 12);  // ★ 最低 12 步，消除大步长断层

    float stepSize = rayLen / float(steps);

    // ★ 抖动：用像素世界坐标生成伪随机偏移，打破规则网格
    float jitter = hash21(worldPos.xy * 97.0 + worldPos.z * 53.0 + time * 13.0) * 0.8;

    float accum = 0.0;
    for (int i = 0; i < steps; ++i) {
        float t = (float(i) + 0.5 + jitter) * stepSize;
        t = clamp(t, 0.0, rayLen);
        vec3 samplePos = camPos + rayDir * t;

        float tSun = (uCloudPlaneHeight - samplePos.z) / max(abs(sunDir.z), 1e-6);
        if (tSun > 0.0) {
            vec3 hitPoint = samplePos + tSun * sunDir;
            // ★ 关键：体积光累加使用 RAW Perlin 值，不做 banding 量化
            //     banding 只对最终结果做一次，避免步间跳变
            vec2 uv = vec2(hitPoint.x, hitPoint.y) * uCloudScale;
            uv.x += time * uCloudSpeed * 0.3;
            uv.y += time * uCloudSpeed * 0.15;
            float cloud = fbm3D(vec3(uv, time * 0.005));
            accum += 1.0 - smoothstep(uCloudThreshold - 0.05,
                                      uCloudThreshold + 0.05, cloud);
        } else {
            accum += 1.0;
        }
    }

    float result = accum / float(steps);  // [0, 1]

    // ★ Banding 只在最终值上做一次，不在每步中间做
    if (uCloudBandLevels > 1.5) {
        result = (floor(result * uCloudBandLevels) + 0.5) / uCloudBandLevels;
    }

    return result;
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

    // ---- 云阴影 (头顶直接遮挡) ----
    float cloudShadow = getCloudShadow(worldPos, Ldir, uCloudTime);

    // ---- 体积光 (大气透射率) ----
    float volumetric = getVolumetricLight(worldPos, uCamPos, Ldir, uCloudTime);

    // ★ 组合：云阴影 × 体积光 = 真正到达该表面点的阳光比例
    float atmosphereLight = cloudShadow * (0.35 + 0.65 * volumetric);

    // ★ 方向光被 atmosphereLight 调制 → 体积光真正参与 BRDF 照明
    vec3 dirRad = uLightColor * uLightIntensity * atmosphereLight;
    vec3 direct = evalBRDF(N, V, Ldir, baseColor, roughness, metallic, F0, dirRad);

    // SSS 同样受大气透射影响
    float backLight = max(dot(-N, Ldir), 0.0);
    vec3 sssRadiance = uLightColor * uLightIntensity * atmosphereLight * 0.5;
    vec3 sssTerm = sss * backLight * baseColor * sssRadiance;

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

    // Ambient
    float upDot = clamp(N.z * 0.5 + 0.5, 0.0, 1.0);
    vec3 hemi = mix(uGroundColor, uSkyColor, upDot);
    vec3 envIrradiance = uAmbient + hemi;
    vec3 ambientDiffuse = (1.0 - metallic) * baseColor * envIrradiance;
    vec3 F_env = F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - NoV, 5.0);
    vec3 ambientSpec = F_env * envIrradiance;
    vec3 ambientTerm = ambientDiffuse + ambientSpec;

    // ============================================================
    // ★ 金光闪闪的体积光
    // ============================================================

    // ---- 1. 纯金色调 (去掉冷白，往暖金方向偏移) ----
    vec3 goldCore  = vec3(1.00, 0.82, 0.35);   // 中心：浓金
    vec3 goldHalo  = vec3(1.00, 0.62, 0.18);   // 边缘：橙金 — 更暖、更暗
    vec3 goldSpark = vec3(1.00, 0.92, 0.60);   // 高亮闪烁色：浅金

    // ---- 2. 非线性辉光曲线：让光束"凝聚"成金色光柱 ----
    // pow(x, 1.2) 让弱光区域更多金色渐变
    // pow(x, 4.0) 让强光部分收窄成锐利光柱
    float beamSoft  = pow(volumetric, 1.2);   // 软光晕
    float beamTight = pow(volumetric, 4.0);   // 紧致光柱

    // ---- 3. 空间闪烁噪声：模拟"金粉飞舞" ----
    float sparkleNoise = fract(
        sin(dot(worldPos.xy * 17.0, vec2(12.9898, 78.233)))
      + sin(dot(worldPos.yz * 23.0, vec2(45.164, 93.497)))
      + uCloudTime * 0.5                                    // 随时间流动
    );
    // 只在强光区域出现闪烁（避免暗处也有金粉）
    float sparkleMask = smoothstep(0.25, 0.65, volumetric);
    float sparkle = sparkleNoise * sparkleMask;

    // ---- 4. 组合三层 ----
    //   A. 金色软光晕（大面积暖色氛围）
    //   B. 紧致光柱（光束中心的高亮金线）
    //   C. 闪烁金粉（高频亮点）
    vec3 inscatter =
          goldHalo  * beamSoft  * 0.15   // 软光晕：低强度，大面积
        + goldCore  * beamTight * 0.30   // 光柱中心：中高强度
        + goldSpark * sparkle   * 0.12;  // 闪烁：纯加法

    inscatter *= uVolumetricIntensity * uLightIntensity;


    // Combine — ★ 注意这里用的是 inscatter
    vec3 col = direct + pointSum + sssTerm + ambientTerm + emissive + inscatter;

    // Fog
    if (uFogDensity > 0.0) {
        float dPl = length(uPlayerPos.xy - worldPos.xy);
        float dFog = max(dPl - uFogStart, 0.0);
        float heightK = exp(-max(worldPos.z - uPlayerPos.z, 0.0) * uFogHeightFalloff);
        float fogAmt = 1.0 - exp(-dFog * uFogDensity * heightK);
        col = mix(col, uFogColor, clamp(fogAmt, 0.0, 1.0));
    }

    

    // Reinhard tonemap + Gamma
    col = col / (col + vec3(1.0));
    col = pow(col, vec3(1.0 / 2.2));

    // Shade levels
    if (uShadeLevels > 1.5) {
        float Y = max(dot(col, vec3(0.2126, 0.7152, 0.0722)), 1e-5);
        float Yq = (floor(Y * uShadeLevels) + 0.5) / uShadeLevels;
        col = col * (Yq / Y);
    }

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
