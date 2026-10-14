#if defined(BKLV_OPENGL) || defined(BKLV_GLES3)
#include "bklv/backends/gl_glass_shaders.hpp"

namespace bklv::glass::shaders
{

// 版本头单独给：桌面是 GLSL 150，Switch 是 GLSL ES 3.0（要写 precision）。
// #version 必须是源码第一行，所以由 linkProgram 拼在最前面。
#if defined(BKLV_GLES3)
const char* const kGlslHeader = "#version 300 es\nprecision highp float;\n"
                                "precision highp sampler2D;\n";
#else
const char* const kGlslHeader = "#version 150\n";
#endif

// ---------------------------------------------------------------------------
// 全屏三角形（无顶点属性，位置由 gl_VertexID 生成）
// ---------------------------------------------------------------------------
const char* const kFullscreenVertex = R"GLSL(
out vec2 vUv;

const vec2 positions[3] = vec2[3](
    vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
const vec2 uvs[3] = vec2[3](
    vec2(0.0, 0.0), vec2(2.0, 0.0), vec2(0.0, 2.0));

void main() {
  gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
  vUv = uvs[gl_VertexID];
}
)GLSL";

// ---------------------------------------------------------------------------
// 模糊：第一趟做 2x2 box 降采样，后两趟做可分离高斯（双线性配对采样）
// ---------------------------------------------------------------------------
const char* const kBlurFragment = R"GLSL(
uniform sampler2D uSource;
uniform vec2 uInverseSourceResolution;
uniform vec2 uDirection;
uniform float uCenterWeight;
uniform float uPairCount;
uniform float uPassKind;
uniform float uPairWeights[12];
uniform float uPairOffsets[12];

in vec2 vUv;
out vec4 outColor;

// ---- blur_kernel（原 common/blur_kernel.glslinc）----
const int kGaussianPairCapacity = 12;

vec4 sampleBoxDown(sampler2D image, vec2 uv, vec2 inverseSourceResolution) {
  vec2 halfTexel = inverseSourceResolution * 0.5;
  vec4 color = texture(image, uv + vec2(-halfTexel.x, -halfTexel.y));
  color += texture(image, uv + vec2( halfTexel.x, -halfTexel.y));
  color += texture(image, uv + vec2(-halfTexel.x,  halfTexel.y));
  color += texture(image, uv + vec2( halfTexel.x,  halfTexel.y));
  return color * 0.25;
}

vec4 sampleGaussian(sampler2D image, vec2 uv,
                    vec2 inverseSourceResolution, vec2 direction,
                    float centerWeight, float pairCount,
                    float pairWeights[kGaussianPairCapacity],
                    float pairOffsets[kGaussianPairCapacity]) {
  vec4 color = texture(image, uv) * centerWeight;
  int count = int(pairCount + 0.5);
  for (int pair = 0; pair < kGaussianPairCapacity; ++pair) {
    if (pair >= count)
      break;
    vec2 offset = direction * inverseSourceResolution * pairOffsets[pair];
    color += texture(image, uv + offset) * pairWeights[pair];
    color += texture(image, uv - offset) * pairWeights[pair];
  }
  return color;
}

void main() {
  if (uPassKind < 0.5) {
    outColor = sampleBoxDown(uSource, vUv, uInverseSourceResolution);
  } else {
    outColor = sampleGaussian(
        uSource, vUv, uInverseSourceResolution, uDirection,
        uCenterWeight, uPairCount, uPairWeights, uPairOffsets);
  }
}
)GLSL";

// ---------------------------------------------------------------------------
// 玻璃几何：一个覆盖"玻璃矩形向外扩阴影范围"的四边形，位置由 gl_VertexID 生成
// ---------------------------------------------------------------------------
const char* const kGlassVertex = R"GLSL(
uniform vec4 uBounds;
uniform vec4 uLight;
uniform vec2 uFramebufferSize;

out vec2 vPixelCoordinate;

const vec2 corners[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0));

void main() {
  vec2 local = corners[gl_VertexID];
  float drawExtent = max(uLight.z, 1.0);
  vec4 drawBounds = uBounds + vec4(vec2(-drawExtent), vec2(drawExtent));
  vec2 position = mix(drawBounds.xy, drawBounds.zw, local);
  vec2 ndc = position / uFramebufferSize * 2.0 - 1.0;
  ndc.y = -ndc.y;
  gl_Position = vec4(ndc, 0.0, 1.0);
  vPixelCoordinate = position;
}
)GLSL";

// ---------------------------------------------------------------------------
// 液态玻璃主体：折射 + 色散 + 染色 + 边缘光照 + 外阴影
// 内联了 glass_math / glass_color / glass_lighting 三个公共模块
// ---------------------------------------------------------------------------
const char* const kGlassFragment = R"GLSL(
// Adapted from AndroidLiquidGlassView@9cdd915; (c) 2025 QmDeve, MIT.

uniform sampler2D uBackdrop;
uniform sampler2D uRawBackdrop;
uniform vec4 uBounds;
uniform vec4 uRadii;
uniform vec4 uRefraction;
uniform vec4 uFilter;
uniform vec4 uTint;
uniform vec4 uFramebuffer;
uniform vec4 uDeformation;
uniform vec4 uLighting;
uniform vec4 uLight;

in vec2 vPixelCoordinate;
out vec4 outColor;

// ---- glass_math（原 common/glass_math.glslinc）----
float radiusAt(vec2 point, vec4 radii) {
  if (point.x >= 0.0)
    return point.y <= 0.0 ? radii.y : radii.z;
  return point.y <= 0.0 ? radii.x : radii.w;
}

float sdRoundedRect(vec2 point, vec2 halfSize, float radius) {
  vec2 corner = abs(point) - (halfSize - vec2(radius));
  float outside = length(max(corner, vec2(0.0))) - radius;
  float inside = min(max(corner.x, corner.y), 0.0);
  return outside + inside;
}

float sdCircle(vec2 point, float radius) {
  return length(point) - radius;
}

float antialiasedCoverage(float signedDistance) {
  float edgeWidth = max(fwidth(signedDistance), 0.5);
  return 1.0 - smoothstep(-edgeWidth, edgeWidth, signedDistance);
}

// Exact pixel coverage for a locally straight edge, including jelly scaling.
float pixelCoverage(float signedDistance, vec2 gradient) {
  float gradientLength = length(gradient);
  if (gradientLength < 1e-4)
    return signedDistance < 0.0 ? 1.0 : 0.0;
  vec2 normal = abs(gradient) / gradientLength;
  float major = max(normal.x, normal.y);
  float minor = min(normal.x, normal.y);
  float offset = -signedDistance / gradientLength;
  float extent = 0.5 * (major + minor);
  if (offset <= -extent)
    return 0.0;
  if (offset >= extent)
    return 1.0;
  if (minor < 1e-4 || abs(offset) <= 0.5 * (major - minor))
    return clamp(0.5 + offset / major, 0.0, 1.0);
  float corner = extent - abs(offset);
  float area = corner * corner / (2.0 * major * minor);
  return offset < 0.0 ? area : 1.0 - area;
}

float safeSign(float value) {
  return value < 0.0 ? -1.0 : 1.0;
}

vec2 safeNormalize(vec2 value, vec2 fallback) {
  float valueLength = length(value);
  return valueLength > 0.001 ? value / valueLength : fallback;
}

vec2 jellyHalfSize(vec2 halfSize, vec2 deformation) {
  vec2 inset = min(abs(deformation) * 0.46, vec2(0.18));
  return max(halfSize * (vec2(1.0) - inset), vec2(1.0));
}

vec2 jellyCoordinate(vec2 point, vec2 halfSize, vec2 deformation) {
  vec2 normalized = clamp(point / max(halfSize, vec2(1.0)),
                          vec2(-1.0), vec2(1.0));
  vec2 profile = vec2(1.0 - normalized.y * normalized.y,
                      1.0 - normalized.x * normalized.x);
  return point - deformation * halfSize * profile * 0.72;
}

vec2 gradSdRoundedRect(vec2 point, vec2 halfSize, float radius) {
  vec2 corner = abs(point) - (halfSize - vec2(radius));
  if (corner.x >= 0.0 || corner.y >= 0.0) {
    vec2 outside = max(corner, vec2(0.0));
    float outsideLength = length(outside);
    if (outsideLength > 0.001)
      return sign(point) * (outside / outsideLength);
    float useX = step(corner.y, corner.x);
    return vec2(useX * safeSign(point.x),
                (1.0 - useX) * safeSign(point.y));
  }
  float useX = step(corner.y, corner.x);
  return sign(point) * vec2(useX, 1.0 - useX);
}

vec2 gradSdCircle(vec2 point) {
  return safeNormalize(point, vec2(1.0, 0.0));
}

float circleMap(float value) {
  float mapped = clamp(value, -1.0, 1.0);
  return 1.0 - sqrt(max(0.0, 1.0 - mapped * mapped));
}

// ---- glass_color（原 common/glass_color.glslinc）----
vec3 toLinearSrgb(vec3 color) {
  vec3 low = color / 12.92;
  vec3 high = pow(max((color + 0.055) / 1.055, vec3(0.0)), vec3(2.4));
  return mix(high, low, lessThanEqual(color, vec3(0.04045)));
}

vec3 fromLinearSrgb(vec3 color) {
  vec3 low = color * 12.92;
  vec3 high = 1.055 * pow(max(color, vec3(0.0)), vec3(1.0 / 2.4)) - 0.055;
  return mix(high, low, lessThanEqual(color, vec3(0.0031308)));
}

vec4 filterLiquidGlassColor(vec4 color, float contrast, float whitePoint,
                            float chromaMultiplier, vec4 tint) {
  const vec3 rgbToY = vec3(0.2126, 0.7152, 0.0722);
  vec3 linear = toLinearSrgb(color.rgb);
  float luminance = dot(linear, rgbToY);
  color.rgb = fromLinearSrgb(
      mix(vec3(luminance), linear, chromaMultiplier));
  vec3 target = whitePoint > 0.0 ? vec3(1.0) : vec3(0.0);
  color.rgb = mix(color.rgb, target, abs(whitePoint));
  color.rgb = (color.rgb - 0.5) * (1.0 + contrast) + 0.5;
  color.rgb = mix(color.rgb, tint.rgb, tint.a);
  return color;
}

// ---- glass_lighting（原 common/glass_lighting.glslinc）----
vec2 glassLightDirection(vec4 light) {
  return safeNormalize(light.xy, vec2(-0.7071, -0.7071));
}

float rimIntegral(float t, float h) {
  float u = 1.0 - sqrt(clamp(t / h, 0.0, 1.0));
  float u6 = u * u * u * u * u * u;
  return 2.0 * h * u6 * (u / 7.0 - 1.0 / 6.0);
}

float filteredRim(float signedDistance, float edgeWidth,
                  vec2 distanceGradient) {
  float h = max(edgeWidth, 0.001);
  float halfPixelWidth =
      0.5 * max(abs(distanceGradient.x) + abs(distanceGradient.y), 1e-4);
  float t0 = max(-signedDistance - halfPixelWidth, 0.0);
  float t1 = max(-signedDistance + halfPixelWidth, 0.0);
  if (t1 <= 0.0)
    return 0.0;
  if (t1 - t0 < 1e-4) {
    float u = 1.0 - sqrt(clamp(t0 / h, 0.0, 1.0));
    return u * u * u * u * u;
  }
  return (rimIntegral(t1, h) - rimIntegral(t0, h)) / (t1 - t0);
}

vec3 applyGlassLighting(vec3 color, float signedDistance, vec2 normal,
                        float edgeWidth, vec2 distanceGradient,
                        vec4 lighting, vec4 light) {
  float rim = filteredRim(signedDistance, edgeWidth, distanceGradient);
  vec2 lightDirection = glassLightDirection(light);

  float shadowFacing = max(dot(normal, -lightDirection), 0.0);
  color *= 1.0 - lighting.x * rim * shadowFacing;
  color += lighting.y * rim * (vec3(1.0) - color);

  float specular = pow(max(dot(normal, lightDirection), 0.0),
                       max(light.w, 1.0));
  color += lighting.z * rim * specular * (vec3(1.0) - color);
  return color;
}

float glassOuterShadow(float signedDistance, vec2 normal, vec4 lighting,
                       vec4 light) {
  float extent = max(light.z, 0.001);
  float falloff = 1.0 - smoothstep(0.0, extent, signedDistance);
  float direction = max(dot(normal, -glassLightDirection(light)), 0.0);
  return lighting.w * falloff * mix(0.35, 1.0, direction);
}

// ---- 主体 ----
vec4 sampleContent(vec2 pixelCoordinate) {
  vec2 halfTexel = 0.5 / vec2(textureSize(uBackdrop, 0));
  vec2 uv = clamp(pixelCoordinate / uFramebuffer.xy, halfTexel,
                  vec2(1.0) - halfTexel);
  uv.y = 1.0 - uv.y;
  float blurMix = clamp(uDeformation.z, 0.0, 1.0);
  if (blurMix >= 0.999)
    return texture(uBackdrop, uv);
  if (blurMix <= 0.001)
    return texture(uRawBackdrop, uv);
  return mix(texture(uRawBackdrop, uv), texture(uBackdrop, uv), blurMix);
}

vec4 applyColorFilter(vec4 color) {
  color = filterLiquidGlassColor(color, uFilter.x, uFilter.y,
                                 uFilter.z, uTint);
  color.a = uFilter.w;
  return color;
}

void main() {
  vec2 size = max(uBounds.zw - uBounds.xy, vec2(1.0));
  vec2 halfSize = size * 0.5;
  vec2 centeredCoordinate = vPixelCoordinate - (uBounds.xy + halfSize);
  bool isCircle = uFramebuffer.w > 0.5;
  vec2 shapeHalfSize = jellyHalfSize(halfSize, uDeformation.xy);
  vec2 shapeCoordinate =
      jellyCoordinate(centeredCoordinate, halfSize, uDeformation.xy);
  float radius = min(radiusAt(shapeCoordinate, uRadii),
                     min(shapeHalfSize.x, shapeHalfSize.y));
  float signedDistance = isCircle
      ? sdCircle(shapeCoordinate, min(shapeHalfSize.x, shapeHalfSize.y))
      : sdRoundedRect(shapeCoordinate, shapeHalfSize, radius);
  float smoothRadius = min(max(radius * 1.5, 30.0),
                           min(shapeHalfSize.x, shapeHalfSize.y));
  vec2 shapeGradient = isCircle
      ? gradSdCircle(shapeCoordinate)
      : gradSdRoundedRect(shapeCoordinate, shapeHalfSize, smoothRadius);

  // Resolve derivatives and both sides of the edge before any early return.
  vec2 distanceGradient =
      vec2(dFdx(signedDistance), dFdy(signedDistance));
  float coverage = min(size.x, size.y) <= 128.0
                       ? pixelCoverage(signedDistance, distanceGradient)
                       : antialiasedCoverage(signedDistance);
  float shadow = uLighting.w > 0.0 && signedDistance < uLight.z
                     ? glassOuterShadow(max(signedDistance, 0.0),
                                        shapeGradient, uLighting, uLight)
                     : 0.0;
  if (coverage <= 0.0) {
    if (shadow <= 0.001)
      discard;
    outColor = vec4(0.0, 0.0, 0.0, shadow * uFilter.w);
    return;
  }

  float shadeDistance = min(signedDistance, -0.001);
  float refractionHeight = max(uRefraction.x, 0.001);
  vec4 color;
  if (-shadeDistance >= refractionHeight) {
    color = sampleContent(vPixelCoordinate);
  } else {
    float depth = 1.0 - (-shadeDistance / refractionHeight);
    float displacement = circleMap(depth) * uRefraction.y;
    vec2 depthGradient = safeNormalize(shapeCoordinate, shapeGradient);
    vec2 gradient = safeNormalize(
        shapeGradient + uRefraction.z * depthGradient, shapeGradient);
    vec2 refractedCoordinate = vPixelCoordinate + displacement * gradient;
    float dispersionIntensity =
        uRefraction.w *
        ((shapeCoordinate.x * shapeCoordinate.y) /
         max(shapeHalfSize.x * shapeHalfSize.y, 0.001));
    vec2 dispersedCoordinate =
        displacement * gradient * dispersionIntensity;

    color = vec4(0.0);
    vec4 red = sampleContent(refractedCoordinate + dispersedCoordinate);
    color.r += red.r / 3.5;
    color.a += red.a / 7.0;
    vec4 orange = sampleContent(
        refractedCoordinate + dispersedCoordinate * (2.0 / 3.0));
    color.r += orange.r / 3.5;
    color.g += orange.g / 7.0;
    color.a += orange.a / 7.0;
    vec4 yellow = sampleContent(
        refractedCoordinate + dispersedCoordinate * (1.0 / 3.0));
    color.r += yellow.r / 3.5;
    color.g += yellow.g / 3.5;
    color.a += yellow.a / 7.0;
    vec4 green = sampleContent(refractedCoordinate);
    color.g += green.g / 3.5;
    color.a += green.a / 7.0;
    vec4 cyan = sampleContent(
        refractedCoordinate - dispersedCoordinate * (1.0 / 3.0));
    color.g += cyan.g / 3.5;
    color.b += cyan.b / 3.0;
    color.a += cyan.a / 7.0;
    vec4 blue = sampleContent(
        refractedCoordinate - dispersedCoordinate * (2.0 / 3.0));
    color.b += blue.b / 3.0;
    color.a += blue.a / 7.0;
    vec4 purple = sampleContent(refractedCoordinate - dispersedCoordinate);
    color.r += purple.r / 7.0;
    color.b += purple.b / 3.0;
    color.a += purple.a / 7.0;
  }

  color = applyColorFilter(color);
  color.rgb = applyGlassLighting(color.rgb, signedDistance, shapeGradient,
                                 refractionHeight, distanceGradient,
                                 uLighting, uLight);
  float glassAlpha = color.a * coverage;
  float alpha = glassAlpha + shadow * uFilter.w * (1.0 - coverage);
  outColor = vec4(color.rgb * (glassAlpha / max(alpha, 1e-6)), alpha);
}
)GLSL";

} // namespace bklv::glass::shaders

#endif // BKLV_OPENGL || BKLV_GLES3
