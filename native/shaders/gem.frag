#version 440
// The gem's frames (web/scripts/gem-frames.mjs) are rendered on black: brightness is coverage, so
// the stone and its glow add light to the panel behind, like the site's screen blend. Two
// neighboring frames are mixed by t, so the turn reads smoother than the frame count.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float t;
};
layout(binding = 1) uniform sampler2D a;
layout(binding = 2) uniform sampler2D b;
void main()
{
    vec3 c = mix(texture(a, qt_TexCoord0).rgb, texture(b, qt_TexCoord0).rgb, t);
    fragColor = vec4(c, max(c.r, max(c.g, c.b))) * qt_Opacity; // premultiplied: rgb <= alpha
}
