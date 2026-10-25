#version 450

// Minimal shader fixture for LibrashaderParameters.EnumerateFixturePreset, so Enumerate has
// hermetic coverage. The two #pragma parameter defaults (test_gamma=2.5, test_flag=1.0)
// deliberately differ from the .slangp overrides (2.4, 0.0) so the test can tell which source
// librashader reports in p.initial.

#pragma parameter test_gamma "Test Gamma" 2.500000 1.000000 5.000000 0.025000
#pragma parameter test_flag "Test Flag" 1.000000 0.000000 1.000000 1.000000

layout(push_constant) uniform Push
{
   float test_gamma;
   float test_flag;
} params;

#pragma stage vertex
layout(location = 0) in vec4 Position;
layout(location = 1) in vec2 TexCoord;
layout(location = 0) out vec2 vTexCoord;
void main()
{
   gl_Position = Position;
   vTexCoord = TexCoord;
}

#pragma stage fragment
layout(location = 0) in vec2 vTexCoord;
layout(location = 0) out vec4 FragColor;
layout(set = 0, binding = 2) uniform sampler2D Source;
void main()
{
   FragColor = vec4(pow(texture(Source, vTexCoord).rgb, vec3(params.test_gamma)), 1.0);
}
